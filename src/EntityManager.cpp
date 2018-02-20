/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include <Utils.hpp>
#include <dbus/properties.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <future>
#include <regex>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <dbus/connection.hpp>
#include <VariantVisitors.hpp>
#include <experimental/filesystem>

constexpr const char *OUTPUT_DIR = "/var/configuration/";
constexpr const char *CONFIGURATION_DIR = "/usr/share/configurations";
constexpr const char *TEMPLATE_CHAR = "$";
constexpr const size_t PROPERTIES_CHANGED_UNTIL_FLUSH = 20;
constexpr const size_t MAX_MAPPER_DEPTH = 99;

namespace fs = std::experimental::filesystem;
struct cmp_str
{
    bool operator()(const char *a, const char *b) const
    {
        return std::strcmp(a, b) < 0;
    }
};

// underscore T for collison with dbus c api
enum class probe_type_codes
{
    FALSE_T,
    TRUE_T,
    AND,
    OR,
    FOUND
};
const static boost::container::flat_map<const char *, probe_type_codes, cmp_str>
    PROBE_TYPES{{{"FALSE", probe_type_codes::FALSE_T},
                 {"TRUE", probe_type_codes::TRUE_T},
                 {"AND", probe_type_codes::AND},
                 {"OR", probe_type_codes::OR},
                 {"FOUND", probe_type_codes::FOUND}}};

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

using ManagedObjectType = boost::container::flat_map<
    dbus::object_path,
    boost::container::flat_map<
        std::string,
        boost::container::flat_map<std::string, dbus::dbus_variant>>>;

boost::container::flat_map<
    std::string,
    std::vector<boost::container::flat_map<std::string, dbus::dbus_variant>>>
    DBUS_PROBE_OBJECTS;
std::vector<std::string> PASSED_PROBES;

// todo: pass this through nicer
std::shared_ptr<dbus::connection> SYSTEM_BUS;

std::regex ILLEGAL_DBUS_REGEX("[^A-Za-z0-9_]");

void registerCallbacks(
    std::vector<std::pair<std::unique_ptr<dbus::match>,
                          std::shared_ptr<dbus::filter>>> &dbusMatches,
    std::atomic_bool &threadRunning, nlohmann::json &systemConfiguration,
    dbus::DbusObjectServer &objServer);

// calls the mapper to find all exposed objects of an interface type
// and creates a vector<flat_map> that contains all the key value pairs
// getManagedObjects
bool findDbusObjects(
    std::shared_ptr<dbus::connection> connection,
    std::vector<boost::container::flat_map<std::string, dbus::dbus_variant>>
        &interfaceDevices,
    std::string interface)
{
    // find all connections in the mapper that expose a specific type
    static const dbus::endpoint mapper("xyz.openbmc_project.ObjectMapper",
                                       "/xyz/openbmc_project/object_mapper",
                                       "xyz.openbmc_project.ObjectMapper",
                                       "GetSubTree");
    dbus::message getMap = dbus::message::new_call(mapper);
    std::vector<std::string> objects = {interface};
    if (!getMap.pack("", MAX_MAPPER_DEPTH, objects))
    {
        std::cerr << "Pack Failed GetSensorSubtree\n";
        return false;
    }
    dbus::message getMapResp = connection->send(getMap);
    GetSubTreeType interfaceSubtree;
    if (!getMapResp.unpack(interfaceSubtree))
    {
        std::cerr << "Error communicating to mapper\n";
        return false;
    }
    boost::container::flat_set<std::string> connections;
    for (auto &object : interfaceSubtree)
    {
        for (auto &connPair : object.second)
        {
            connections.insert(connPair.first);
        }
    }
    // iterate through the connections, adding creating individual device
    // dictionaries
    for (auto &conn : connections)
    {
        auto managedObj =
            dbus::endpoint(conn, "/", "org.freedesktop.DBus.ObjectManager",
                           "GetManagedObjects");
        dbus::message getManagedObj = dbus::message::new_call(managedObj);
        dbus::message getManagedObjResp = connection->send(getManagedObj);
        ManagedObjectType managedInterface;
        if (!getManagedObjResp.unpack(managedInterface))
        {
            std::cerr << "error getting managed object for device " << conn
                      << "\n";
            continue;
        }
        for (auto &interfaceManagedObj : managedInterface)
        {
            auto ifaceObjFind = interfaceManagedObj.second.find(interface);
            if (ifaceObjFind != interfaceManagedObj.second.end())
            {
                interfaceDevices.emplace_back(ifaceObjFind->second);
            }
        }
    }
    return true;
}

// probes interface dictionary for a key with a value that matches a regex
bool probeDbus(
    const std::string &interface,
    const std::map<std::string, nlohmann::json> &matches,
    std::vector<boost::container::flat_map<std::string, dbus::dbus_variant>>
        &devices,
    bool &foundProbe)
{
    auto &dbusObject = DBUS_PROBE_OBJECTS[interface];
    if (dbusObject.empty())
    {
        if (!findDbusObjects(SYSTEM_BUS, dbusObject, interface))
        {
            std::cerr << "Found no dbus objects with interface "
                      << interface << "\n";
            foundProbe = false;
            return false;
        }
    }
    foundProbe = true;

    bool foundMatch = false;
    for (auto &device : dbusObject)
    {
        bool deviceMatches = true;
        for (auto &match : matches)
        {
            auto deviceValue = device.find(match.first);
            if (deviceValue != device.end())
            {
                switch (match.second.type())
                {
                case nlohmann::json::value_t::string:
                {
                    std::regex search(match.second.get<std::string>());
                    std::smatch match;

                    // convert value to string respresentation
                    std::string probeValue = boost::apply_visitor(
                        [](const auto &x) {
                            return boost::lexical_cast<std::string>(x);
                        },
                        deviceValue->second);
                    if (!std::regex_search(probeValue, match, search))
                    {
                        deviceMatches = false;
                        break;
                    }
                    break;
                }
                case nlohmann::json::value_t::boolean:
                case nlohmann::json::value_t::number_unsigned:
                {
                    unsigned int probeValue = boost::apply_visitor(
                        VariantToUnsignedIntVisitor(), deviceValue->second);

                    if (probeValue != match.second.get<unsigned int>())
                    {
                        deviceMatches = false;
                    }
                    break;
                }
                case nlohmann::json::value_t::number_integer:
                {
                    int probeValue = boost::apply_visitor(VariantToIntVisitor(),
                                                          deviceValue->second);

                    if (probeValue != match.second.get<int>())
                    {
                        deviceMatches = false;
                    }
                    break;
                }
                case nlohmann::json::value_t::number_float:
                {
                    float probeValue = boost::apply_visitor(
                        VariantToFloatVisitor(), deviceValue->second);

                    if (probeValue != match.second.get<float>())
                    {
                        deviceMatches = false;
                    }
                    break;
                }
                }
            }
            else
            {
                deviceMatches = false;
                break;
            }
        }
        if (deviceMatches)
        {
            devices.emplace_back(
                boost::container::flat_map<std::string, dbus::dbus_variant>(
                    device));
            foundMatch = true;
            deviceMatches = false; // for next iteration
        }
    }
    return foundMatch;
}

// default probe entry point, iterates a list looking for specific types to
// call specific probe functions
bool probe(
    const std::vector<std::string> probeCommand,
    std::vector<boost::container::flat_map<std::string, dbus::dbus_variant>>
        &foundDevs)
{
    const static std::regex command(R"(\((.*)\))");
    std::smatch match;
    bool ret = false;
    bool cur = true;
    probe_type_codes lastCommand = probe_type_codes::FALSE_T;

    for (auto &probe : probeCommand)
    {
        bool foundProbe = false;
        boost::container::flat_map<const char *, probe_type_codes,
                                   cmp_str>::const_iterator probeType;

        for (probeType = PROBE_TYPES.begin(); probeType != PROBE_TYPES.end();
             probeType++)
        {
            if (probe.find(probeType->first) != std::string::npos)
            {
                foundProbe = true;
                break;
            }
        }
        if (foundProbe)
        {
            switch (probeType->second)
            {
            case probe_type_codes::FALSE_T:
            {
                return false; // todo, actually evaluate?
                break;
            }
            case probe_type_codes::TRUE_T:
            {
                return true; // todo, actually evaluate?
                break;
            }
            /*case probe_type_codes::AND:
              break;
            case probe_type_codes::OR:
              break;
              // these are no-ops until the last command switch
              */
            case probe_type_codes::FOUND:
            {
                if (!std::regex_search(probe, match, command))
                {
                    std::cerr << "found probe sytax error " << probe << "\n";
                    return false;
                }
                std::string commandStr = *(match.begin() + 1);
                boost::replace_all(commandStr, "'", "");
                cur = (std::find(PASSED_PROBES.begin(), PASSED_PROBES.end(),
                                 commandStr) != PASSED_PROBES.end());
                break;
            }
            }
        }
        // look on dbus for object
        else
        {
            if (!std::regex_search(probe, match, command))
            {
                std::cerr << "dbus probe sytax error " << probe << "\n";
                return false;
            }
            std::string commandStr = *(match.begin() + 1);
            // convert single ticks and single slashes into legal json
            boost::replace_all(commandStr, "'", R"(")");
            boost::replace_all(commandStr, R"(\)", R"(\\)");
            auto json = nlohmann::json::parse(commandStr, nullptr, false);
            if (json.is_discarded())
            {
                std::cerr << "dbus command sytax error " << commandStr << "\n";
                return false;
            }
            // we can match any (string, variant) property. (string, string)
            // does a regex
            std::map<std::string, nlohmann::json> dbusProbeMap =
                json.get<std::map<std::string, nlohmann::json>>();
            auto findStart = probe.find("(");
            if (findStart == std::string::npos)
            {
                return false;
            }
            std::string probeInterface = probe.substr(0, findStart);
            cur =
                probeDbus(probeInterface, dbusProbeMap, foundDevs, foundProbe);
        }

        // some functions like AND and OR only take affect after the
        // fact
        switch (lastCommand)
        {
        case probe_type_codes::AND:
            ret = cur && ret;
            break;
        case probe_type_codes::OR:
            ret = cur || ret;
            break;
        default:
            ret = cur;
            break;
        }
        lastCommand = probeType != PROBE_TYPES.end()
                          ? probeType->second
                          : probe_type_codes::FALSE_T;

        if (!foundProbe)
        {
            std::cerr << "Illegal probe type " << probe << "\n";
            return false;
        }
    }

    // probe passed, but empty device
    // todo: should this be done in main?
    if (ret && foundDevs.size() == 0)
    {
        foundDevs.emplace_back(
            boost::container::flat_map<std::string, dbus::dbus_variant>());
    }
    return ret;
}

// this function is temporary, no need to have once dbus is solified.
void writeJsonFiles(nlohmann::json &systemConfiguration)
{
    std::experimental::filesystem::create_directory(OUTPUT_DIR);
    std::ofstream output(std::string(OUTPUT_DIR) + "system.json");
    output << systemConfiguration.dump(4);
    output.close();

    auto flat = nlohmann::json::array();
    for (auto &pair : nlohmann::json::iterator_wrapper(systemConfiguration))
    {
        auto value = pair.value();
        auto exposes = value.find("exposes");
        if (exposes != value.end())
        {
            for (auto &item : *exposes)
            {
                flat.push_back(item);
            }
        }
    }
    output = std::ofstream(std::string(OUTPUT_DIR) + "flattened.json");
    output << flat.dump(4);
    output.close();
}
// adds simple json types to interface's properties
void populateInterfaceFromJson(dbus::DbusInterface *iface, nlohmann::json dict,
                               dbus::DbusObjectServer &objServer)
{
    std::vector<std::pair<std::string, dbus::dbus_variant>> properties;
    static size_t flushCount = 0;

    for (auto &dictPair : nlohmann::json::iterator_wrapper(dict))
    {
        switch (dictPair.value().type())
        {
        case (nlohmann::json::value_t::boolean):
        {
            properties.emplace_back(std::string(dictPair.key()),
                                    dictPair.value().get<bool>());
            break;
        }
        case (nlohmann::json::value_t::number_integer):
        {
            properties.emplace_back(std::string(dictPair.key()),
                                    dictPair.value().get<int64_t>());
            break;
        }
        case (nlohmann::json::value_t::number_unsigned):
        {
            properties.emplace_back(std::string(dictPair.key()),
                                    dictPair.value().get<uint64_t>());
            break;
        }
        case (nlohmann::json::value_t::number_float):
        {
            properties.emplace_back(std::string(dictPair.key()),
                                    dictPair.value().get<float>());
            break;
        }
        case (nlohmann::json::value_t::string):
        {
            properties.emplace_back(std::string(dictPair.key()),
                                    dictPair.value().get<std::string>());
            break;
        }
        }
    }
    if (!properties.empty())
    {
        iface->set_properties(properties);

        // flush the queue after adding an amount of properties so we don't hang
        if (flushCount++ > PROPERTIES_CHANGED_UNTIL_FLUSH)
        {
            objServer.flush();
            flushCount = 0;
        }
    }
}

void postToDbus(const nlohmann::json &systemConfiguration,
                dbus::DbusObjectServer &objServer)

{
    for (auto &boardPair :
         nlohmann::json::iterator_wrapper(systemConfiguration))
    {
        std::string boardKey = boardPair.key();
        auto boardValues = boardPair.value();
        auto findBoardType = boardValues.find("type");
        std::string boardType;
        if (findBoardType != boardValues.end() &&
            findBoardType->type() == nlohmann::json::value_t::string)
        {
            boardType = findBoardType->get<std::string>();
            std::regex_replace(boardType.begin(), boardType.begin(),
                               boardType.end(), ILLEGAL_DBUS_REGEX, "_");
        }
        else
        {
            std::cerr << "Unable to find type for " << boardKey
                      << " reverting to Chassis.\n";
            boardType = "Chassis";
        }

        std::regex_replace(boardKey.begin(), boardKey.begin(), boardKey.end(),
                           ILLEGAL_DBUS_REGEX, "_");
        std::string boardName =
            "/xyz/openbmc_project/Inventory/Item/" + boardType + "/" + boardKey;
        auto boardObject = objServer.add_object(boardName);

        auto boardIface = boardObject->add_interface(
            "xyz.openbmc_project.Configuration." + boardType);
        populateInterfaceFromJson(boardIface.get(), boardValues, objServer);
        auto exposes = boardValues.find("exposes");
        if (exposes == boardValues.end())
        {
            continue;
        }
        for (auto &item : *exposes)
        {
            auto findName = item.find("name");
            if (findName == item.end())
            {
                std::cerr << "cannot find name in field " << item << "\n";
                continue;
            }
            auto findStatus = item.find("status");
            // if status is not found it is assumed to be status = 'okay'
            if (findStatus != item.end())
            {
                if (*findStatus == "disabled")
                {
                    continue;
                }
            }
            auto findType = item.find("type");
            std::string itemType;
            if (findType != item.end())
            {
                itemType = findType->get<std::string>();
                std::regex_replace(itemType.begin(), itemType.begin(),
                                   itemType.end(), ILLEGAL_DBUS_REGEX, "_");
            }
            else
            {
                itemType = "unknown";
            }
            std::string itemName = findName->get<std::string>();
            std::regex_replace(itemName.begin(), itemName.begin(),
                               itemName.end(), ILLEGAL_DBUS_REGEX, "_");
            auto itemObject = objServer.add_object(boardName + "/" + itemName);
            auto itemIface = itemObject->add_interface(
                "xyz.openbmc_project.Configuration." + itemType);

            populateInterfaceFromJson(itemIface.get(), item, objServer);

            for (auto &objectPair : nlohmann::json::iterator_wrapper(item))
            {
                if (objectPair.value().type() ==
                    nlohmann::json::value_t::object)
                {
                    auto objectIface = itemObject->add_interface(
                        "xyz.openbmc_project.Configuration." + itemType + "." +
                        objectPair.key());
                    populateInterfaceFromJson(objectIface.get(),
                                              objectPair.value(), objServer);
                }
                else if (objectPair.value().type() ==
                         nlohmann::json::value_t::array)
                {
                    size_t index = 0;
                    for (auto &arrayItem : objectPair.value())
                    {
                        if (arrayItem.type() != nlohmann::json::value_t::object)
                        {
                            std::cerr << "dbus format error" << arrayItem
                                      << "\n";
                            break;
                        }
                        auto objectIface = itemObject->add_interface(
                            "xyz.openbmc_project.Configuration." + itemType +
                            "." + objectPair.key() + "." +
                            std::to_string(index));
                        index++;
                        populateInterfaceFromJson(objectIface.get(), arrayItem,
                                                  objServer);
                    }
                }
            }
        }
    }
}

// finds the template character (currently set to $) and replaces the value with
// the field found in a dbus object i.e. $ADDRESS would get populated with the
// ADDRESS field from a object on dbus
void templateCharReplace(
    nlohmann::json::iterator &keyPair,
    const boost::container::flat_map<std::string, dbus::dbus_variant>
        &foundDevice,
    size_t &foundDeviceIdx)
{
    if (keyPair.value().type() != nlohmann::json::value_t::string)
    {
        return;
    }

    std::string value = keyPair.value();
    if (value.find(TEMPLATE_CHAR) != std::string::npos)
    {
        std::string templateValue = value;

        templateValue.erase(0, 1); // remove template character

        // special case index
        if ("index" == templateValue)
        {
            keyPair.value() = foundDeviceIdx;
        }
        else
        {
            std::string subsitute;
            for (auto &foundDevicePair : foundDevice)
            {
                if (boost::iequals(foundDevicePair.first, templateValue))
                {
                    // convert value to string
                    // respresentation
                    subsitute = boost::apply_visitor(
                        [](const auto &x) {
                            return boost::lexical_cast<std::string>(x);
                        },
                        foundDevicePair.second);
                    break;
                }
            }
            if (!subsitute.size())
            {
                std::cerr << "could not find symbol " << templateValue << "\n";
            }
            else
            {
                keyPair.value() = subsitute;
            }
        }
    }
}

bool findJsonFiles(std::vector<nlohmann::json> &configurations)
{
    // find configuration files
    std::vector<fs::path> jsonPaths;
    if (!find_files(fs::path(CONFIGURATION_DIR), R"(.*\.json)", jsonPaths, 0))
    {
        std::cerr << "Unable to find any configuration files in "
                  << CONFIGURATION_DIR << "\n";
        return false;
    }
    for (auto &jsonPath : jsonPaths)
    {
        std::ifstream jsonStream(jsonPath.c_str());
        if (!jsonStream.good())
        {
            std::cerr << "unable to open " << jsonPath.string() << "\n";
            continue;
        }
        auto data = nlohmann::json::parse(jsonStream, nullptr, false);
        if (data.is_discarded())
        {
            std::cerr << "syntax error in " << jsonPath.string() << "\n";
            continue;
        }
        if (data.type() == nlohmann::json::value_t::array)
        {
            for (auto &d : data)
            {
                configurations.emplace_back(d);
            }
        }
        else
        {
            configurations.emplace_back(data);
        }
    }
}

bool rescan(nlohmann::json &systemConfiguration)
{
    std::vector<nlohmann::json> configurations;
    if (!findJsonFiles(configurations))
    {
        false;
    }
    // preprocess already passed configurations and missing fields
    if (systemConfiguration.size())
    {
        for (auto it = configurations.begin(); it != configurations.end();)
        {
            auto findName = it->find("name");
            if (findName == it->end())
            {
                std::cerr << "configuration missing name field " << *it << "\n";
                it = configurations.erase(it);
                continue;
            }
            else if (findName->type() != nlohmann::json::value_t::string)
            {
                std::cerr << "name field must be a string " << *findName
                          << "\n";
                it = configurations.erase(it);
                continue;
            }
            auto findAlreadyFound =
                systemConfiguration.find(findName->get<std::string>());
            if (findAlreadyFound != systemConfiguration.end())
            {
                it = configurations.erase(it);
                continue;
            }
            // TODO: add in tags to determine if configuration should be
            // refreshed on AC / DC / Always.
            it++;
        }
    }

    // probe until no probes pass
    bool probePassed = true;
    while (probePassed)
    {
        probePassed = false;
        for (auto it = configurations.begin(); it != configurations.end();)
        {
            bool eraseConfig = false;
            auto findProbe = it->find("probe");
            auto findName = it->find("name");

            nlohmann::json probeCommand;
            // check for poorly formatted fields, probe must be an array
            if (findProbe == it->end())
            {
                std::cerr << "configuration file missing probe:\n " << *it
                          << "\n";
                eraseConfig = true;
            }
            else if ((*findProbe).type() != nlohmann::json::value_t::array)
            {
                probeCommand = nlohmann::json::array();
                probeCommand.push_back(*findProbe);
            }
            else
            {
                probeCommand = *findProbe;
            }

            if (findName == it->end())
            {
                std::cerr << "configuration file missing name:\n " << *it
                          << "\n";
                eraseConfig = true;
            }

            std::vector<
                boost::container::flat_map<std::string, dbus::dbus_variant>>
                foundDevices;
            if (!eraseConfig && probe(probeCommand, foundDevices))
            {
                eraseConfig = true;
                probePassed = true;
                std::string name = *findName;
                PASSED_PROBES.push_back(name);

                size_t foundDeviceIdx = 0;

                for (auto &foundDevice : foundDevices)
                {
                    for (auto keyPair = it->begin(); keyPair != it->end();
                         keyPair++)
                    {
                        templateCharReplace(keyPair, foundDevice,
                                            foundDeviceIdx);
                    }
                    auto findExpose = it->find("exposes");
                    if (findExpose == it->end())
                    {
                        continue;
                    }
                    for (auto &expose : *findExpose)
                    {
                        for (auto keyPair = expose.begin();
                             keyPair != expose.end(); keyPair++)
                        {

                            // fill in template characters with devices
                            // found
                            templateCharReplace(keyPair, foundDevice,
                                                foundDeviceIdx);
                            // special case bind
                            if (boost::starts_with(keyPair.key(), "bind_"))
                            {
                                if (keyPair.value().type() !=
                                    nlohmann::json::value_t::string)
                                {
                                    std::cerr
                                        << "bind_ value must be of type string "
                                        << keyPair.key() << "\n";
                                    continue;
                                }
                                bool foundBind = false;
                                std::string bind =
                                    keyPair.key().substr(sizeof("bind_") - 1);
                                for (auto &configurationPair :
                                     nlohmann::json::iterator_wrapper(
                                         systemConfiguration))
                                {

                                    auto configListFind =
                                        configurationPair.value().find(
                                            "exposes");

                                    if (configListFind ==
                                            configurationPair.value().end() ||
                                        configListFind->type() !=
                                            nlohmann::json::value_t::array)
                                    {
                                        continue;
                                    }
                                    for (auto &exposedObject : *configListFind)
                                    {
                                        std::string foundObjectName =
                                            (exposedObject)["name"];
                                        if (boost::iequals(
                                                foundObjectName,
                                                keyPair.value()
                                                    .get<std::string>()))
                                        {
                                            expose[bind] = exposedObject;
                                            expose[bind]["status"] = "okay";

                                            foundBind = true;
                                            break;
                                        }
                                    }
                                    if (foundBind)
                                    {
                                        break;
                                    }
                                }
                                if (!foundBind)
                                {
                                    std::cerr << "configuration file "
                                                 "dependency error, "
                                                 "could not find bind "
                                              << keyPair.value() << "\n";
                                }
                            }
                        }
                    }
                }
                systemConfiguration[name] = (*it);
                foundDeviceIdx++;
            }

            if (eraseConfig)
            {
                it = configurations.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
}

void propertiesChangedCallback(
    std::vector<std::pair<std::unique_ptr<dbus::match>,
                          std::shared_ptr<dbus::filter>>> &dbusMatches,
    std::atomic_bool &threadRunning, nlohmann::json &systemConfiguration,
    dbus::DbusObjectServer &objServer, std::shared_ptr<dbus::filter> dbusFilter)
{
    static std::future<void> future;
    bool notRunning = false;
    if (threadRunning.compare_exchange_strong(notRunning, true))
    {
        future = std::async(std::launch::async, [&] {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            auto oldConfiguration = systemConfiguration;
            DBUS_PROBE_OBJECTS.clear();
            rescan(systemConfiguration);
            auto newConfiguration = systemConfiguration;
            for (auto it = newConfiguration.begin();
                 it != newConfiguration.end();)
            {
                auto findKey = oldConfiguration.find(it.key());
                if (findKey != oldConfiguration.end())
                {
                    it = newConfiguration.erase(it);
                }
                else
                {
                    it++;
                }
            }
            // only post new items to bus for now
            postToDbus(newConfiguration, objServer);
            // this line to be removed in future
            writeJsonFiles(systemConfiguration);
            registerCallbacks(dbusMatches, threadRunning, systemConfiguration,
                              objServer);
            threadRunning = false;
        });
    }
    if (dbusFilter != nullptr)
    {
        dbusFilter->async_dispatch([&, dbusFilter](boost::system::error_code ec,
                                                   dbus::message) {
            if (ec)
            {
                std::cerr << "properties changed callback error " << ec << "\n";
            }
            propertiesChangedCallback(dbusMatches, threadRunning,
                                      systemConfiguration, objServer,
                                      dbusFilter);
        });
    }
}

void registerCallbacks(
    std::vector<std::pair<std::unique_ptr<dbus::match>,
                          std::shared_ptr<dbus::filter>>> &dbusMatches,
    std::atomic_bool &threadRunning, nlohmann::json &systemConfiguration,
    dbus::DbusObjectServer &objServer)
{
    static boost::container::flat_set<std::string> watchedObjects;

    for (const auto &objectMap : DBUS_PROBE_OBJECTS)
    {
        auto findObject = watchedObjects.find(objectMap.first);
        if (findObject != watchedObjects.end())
        {
            continue;
        }
        // this creates a filter for properties changed for any new probe type
        auto propertyChange = std::make_unique<dbus::match>(
            SYSTEM_BUS,
            "type='signal',member='PropertiesChanged',arg0='" +
                objectMap.first + "'");
        auto filter =
            std::make_shared<dbus::filter>(SYSTEM_BUS, [](dbus::message &m) {
                auto member = m.get_member();
                return member == "PropertiesChanged";
            });

        filter->async_dispatch([&, filter](boost::system::error_code ec,
                                           dbus::message) {
            if (ec)
            {
                std::cerr << "register callbacks callback error " << ec << "\n";
            }
            propertiesChangedCallback(dbusMatches, threadRunning,
                                      systemConfiguration, objServer, filter);
        });
        dbusMatches.emplace_back(std::move(propertyChange), filter);
    }
}

int main(int argc, char **argv)
{
    // setup connection to dbus
    boost::asio::io_service io;
    SYSTEM_BUS = std::make_shared<dbus::connection>(io, dbus::bus::system);
    dbus::DbusObjectServer objServer(SYSTEM_BUS);
    SYSTEM_BUS->request_name("xyz.openbmc_project.EntityManager");

    nlohmann::json systemConfiguration = nlohmann::json::object();
    rescan(systemConfiguration);
    // this line to be removed in future
    writeJsonFiles(systemConfiguration);
    postToDbus(systemConfiguration, objServer);
    auto object = std::make_shared<dbus::DbusObject>(
        SYSTEM_BUS, "/xyz/openbmc_project/EntityManager");
    objServer.register_object(object);
    auto iface = std::make_shared<dbus::DbusInterface>(
        "xyz.openbmc_project.EntityManager", SYSTEM_BUS);
    object->register_interface(iface);

    std::atomic_bool threadRunning(false);
    // to keep reference to the match / filter objects so they don't get
    // destroyed
    std::vector<
        std::pair<std::unique_ptr<dbus::match>, std::shared_ptr<dbus::filter>>>
        dbusMatches;
    iface->register_method("ReScan", [&]() {
        propertiesChangedCallback(dbusMatches, threadRunning,
                                  systemConfiguration, objServer, nullptr);
        return std::tuple<>(); // this is a bug in boost-dbus, needs some sort
                               // of return
    });
    registerCallbacks(dbusMatches, threadRunning, systemConfiguration,
                      objServer);

    io.run();

    return 0;
}
