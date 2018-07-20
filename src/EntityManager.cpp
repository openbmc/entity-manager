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
#include <Overlay.hpp>
#include <nlohmann/json.hpp>
#include <fstream>
#include <regex>
#include <iostream>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <VariantVisitors.hpp>
#include <experimental/filesystem>

constexpr const char *OUTPUT_DIR = "/var/configuration/";
constexpr const char *CONFIGURATION_DIR = "/usr/share/configurations";
constexpr const char *TEMPLATE_CHAR = "$";
constexpr const size_t PROPERTIES_CHANGED_UNTIL_FLUSH_COUNT = 20;
constexpr const int32_t MAX_MAPPER_DEPTH = 0;
constexpr const size_t SLEEP_AFTER_PROPERTIES_CHANGE_SECONDS = 5;

namespace fs = std::experimental::filesystem;
struct cmp_str
{
    bool operator()(const char *a, const char *b) const
    {
        return std::strcmp(a, b) < 0;
    }
};

struct PerformProbe;

// underscore T for collison with dbus c api
enum class probe_type_codes
{
    FALSE_T,
    TRUE_T,
    AND,
    OR,
    FOUND,
    MATCH_ONE
};
const static boost::container::flat_map<const char *, probe_type_codes, cmp_str>
    PROBE_TYPES{{{"FALSE", probe_type_codes::FALSE_T},
                 {"TRUE", probe_type_codes::TRUE_T},
                 {"AND", probe_type_codes::AND},
                 {"OR", probe_type_codes::OR},
                 {"FOUND", probe_type_codes::FOUND},
                 {"MATCH_ONE", probe_type_codes::MATCH_ONE}}};

static constexpr std::array<const char *, 1> SETTABLE_INTERFACES = {
    "thresholds"};

using BasicVariantType =
    sdbusplus::message::variant<std::string, int64_t, uint64_t, double, int32_t,
                                uint32_t, int16_t, uint16_t, uint8_t, bool>;

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

using ManagedObjectType = boost::container::flat_map<
    sdbusplus::message::object_path,
    boost::container::flat_map<
        std::string,
        boost::container::flat_map<std::string, BasicVariantType>>>;

boost::container::flat_map<
    std::string,
    std::vector<boost::container::flat_map<std::string, BasicVariantType>>>
    DBUS_PROBE_OBJECTS;
std::vector<std::string> PASSED_PROBES;

// todo: pass this through nicer
std::shared_ptr<sdbusplus::asio::connection> SYSTEM_BUS;

const std::regex ILLEGAL_DBUS_REGEX("[^A-Za-z0-9_.]");

void registerCallbacks(boost::asio::io_service &io,
                       std::vector<sdbusplus::bus::match::match> &dbusMatches,
                       nlohmann::json &systemConfiguration,
                       sdbusplus::asio::object_server &objServer);

// calls the mapper to find all exposed objects of an interface type
// and creates a vector<flat_map> that contains all the key value pairs
// getManagedObjects
void findDbusObjects(std::shared_ptr<PerformProbe> probe,
                     std::shared_ptr<sdbusplus::asio::connection> connection,
                     std::string &interface)
{
    // todo: this is only static because the mapper is unreliable as of today
    static boost::container::flat_map<std::string,
                                      boost::container::flat_set<std::string>>
        connections;

    // store reference to pending callbacks so we don't overwhelm services
    static boost::container::flat_map<
        std::string, std::vector<std::shared_ptr<PerformProbe>>>
        pendingProbes;

    if (DBUS_PROBE_OBJECTS[interface].size())
    {
        return;
    }

    // add shared_ptr to vector of Probes waiting for callback from a specific
    // interface to keep alive while waiting for response
    std::array<const char *, 1> objects = {interface.c_str()};
    std::vector<std::shared_ptr<PerformProbe>> &pending =
        pendingProbes[interface];
    auto iter = pending.emplace(pending.end(), probe);
    // only allow first call to run to not overwhelm processes
    if (iter != pending.begin())
    {
        return;
    }

    // find all connections in the mapper that expose a specific type
    connection->async_method_call(
        [&, connection, interface](boost::system::error_code &ec,
                                   const GetSubTreeType &interfaceSubtree) {
            auto &interfaceConnections = connections[interface];
            if (ec)
            {
                std::cerr
                    << "Error communicating to mapper, using cached data if "
                       "available\n";
                if (interfaceConnections.empty())
                {
                    return;
                }
            }
            else
            {
                for (auto &object : interfaceSubtree)
                {
                    for (auto &connPair : object.second)
                    {
                        auto insertPair =
                            interfaceConnections.insert(connPair.first);
                    }
                }
            }
            // get managed objects for all interfaces
            for (const auto &conn : interfaceConnections)
            {
                connection->async_method_call(
                    [&, conn,
                     interface](boost::system::error_code &ec,
                                const ManagedObjectType &managedInterface) {
                        if (ec)
                        {
                            std::cerr
                                << "error getting managed object for device "
                                << conn << "\n";
                            pendingProbes[interface].clear();
                            return;
                        }
                        for (auto &interfaceManagedObj : managedInterface)
                        {
                            auto ifaceObjFind =
                                interfaceManagedObj.second.find(interface);
                            if (ifaceObjFind !=
                                interfaceManagedObj.second.end())
                            {
                                std::vector<boost::container::flat_map<
                                    std::string, BasicVariantType>>
                                    &dbusObject = DBUS_PROBE_OBJECTS[interface];
                                dbusObject.emplace_back(ifaceObjFind->second);
                            }
                        }
                        pendingProbes[interface].clear();
                    },
                    conn.c_str(), "/", "org.freedesktop.DBus.ObjectManager",
                    "GetManagedObjects");
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree", "", MAX_MAPPER_DEPTH,
        objects);
}
// probes dbus interface dictionary for a key with a value that matches a regex
bool probeDbus(
    const std::string &interface,
    const std::map<std::string, nlohmann::json> &matches,
    std::vector<boost::container::flat_map<std::string, BasicVariantType>>
        &devices,
    bool &foundProbe)
{
    std::vector<boost::container::flat_map<std::string, BasicVariantType>>
        &dbusObject = DBUS_PROBE_OBJECTS[interface];
    if (dbusObject.empty())
    {
        foundProbe = false;
        return false;
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
                        std::string probeValue = mapbox::util::apply_visitor(
                            VariantToStringVisitor(), deviceValue->second);
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
                        unsigned int probeValue = mapbox::util::apply_visitor(
                            VariantToUnsignedIntVisitor(), deviceValue->second);

                        if (probeValue != match.second.get<unsigned int>())
                        {
                            deviceMatches = false;
                        }
                        break;
                    }
                    case nlohmann::json::value_t::number_integer:
                    {
                        int probeValue = mapbox::util::apply_visitor(
                            VariantToIntVisitor(), deviceValue->second);

                        if (probeValue != match.second.get<int>())
                        {
                            deviceMatches = false;
                        }
                        break;
                    }
                    case nlohmann::json::value_t::number_float:
                    {
                        float probeValue = mapbox::util::apply_visitor(
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
                boost::container::flat_map<std::string, BasicVariantType>(
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
    const std::vector<std::string> &probeCommand,
    std::vector<boost::container::flat_map<std::string, BasicVariantType>>
        &foundDevs)
{
    const static std::regex command(R"(\((.*)\))");
    std::smatch match;
    bool ret = false;
    bool matchOne = false;
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
                    return false;
                    break;
                }
                case probe_type_codes::TRUE_T:
                {
                    return true;
                    break;
                }
                case probe_type_codes::MATCH_ONE:
                {
                    // set current value to last, this probe type shouldn't
                    // affect the outcome
                    cur = ret;
                    matchOne = true;
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
                        std::cerr << "found probe sytax error " << probe
                                  << "\n";
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
            boost::replace_all(commandStr, "'", "\"");
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
    if (ret && foundDevs.size() == 0)
    {
        foundDevs.emplace_back(
            boost::container::flat_map<std::string, BasicVariantType>());
    }
    if (matchOne && foundDevs.size() > 1)
    {
        foundDevs.erase(foundDevs.begin() + 1, foundDevs.end());
    }
    return ret;
}
// this class finds the needed dbus fields and on destruction runs the probe
struct PerformProbe : std::enable_shared_from_this<PerformProbe>
{

    PerformProbe(
        const std::vector<std::string> &probeCommand,
        std::function<void(std::vector<boost::container::flat_map<
                               std::string, BasicVariantType>> &)> &&callback) :
        _probeCommand(probeCommand),
        _callback(std::move(callback))
    {
    }
    ~PerformProbe()
    {
        if (probe(_probeCommand, _foundDevs))
        {
            _callback(_foundDevs);
        }
    }
    void run()
    {
        // parse out dbus probes by discarding other probe types
        boost::container::flat_map<const char *, probe_type_codes,
                                   cmp_str>::const_iterator probeType;

        std::vector<std::string> dbusProbes;
        for (std::string &probe : _probeCommand)
        {
            bool found = false;
            boost::container::flat_map<const char *, probe_type_codes,
                                       cmp_str>::const_iterator probeType;
            for (probeType = PROBE_TYPES.begin();
                 probeType != PROBE_TYPES.end(); probeType++)
            {
                if (probe.find(probeType->first) != std::string::npos)
                {
                    found = true;
                    break;
                }
            }
            if (found)
            {
                continue;
            }
            // syntax requires probe before first open brace
            auto findStart = probe.find("(");
            std::string interface = probe.substr(0, findStart);

            findDbusObjects(shared_from_this(), SYSTEM_BUS, interface);
        }
    }
    std::vector<std::string> _probeCommand;
    std::function<void(
        std::vector<boost::container::flat_map<std::string, BasicVariantType>>
            &)>
        _callback;
    std::vector<boost::container::flat_map<std::string, BasicVariantType>>
        _foundDevs;
};

// writes output files to persist data
void writeJsonFiles(const nlohmann::json &systemConfiguration)
{
    std::experimental::filesystem::create_directory(OUTPUT_DIR);
    std::ofstream output(std::string(OUTPUT_DIR) + "system.json");
    output << systemConfiguration.dump(4);
    output.close();
}

// template function to add array as dbus property
template <typename PropertyType>
void addArrayToDbus(const std::string &name, const nlohmann::json &array,
                    sdbusplus::asio::dbus_interface *iface)
{
    std::vector<PropertyType> values;
    for (const auto &property : array)
    {
        auto ptr = property.get_ptr<const PropertyType *>();
        if (ptr != nullptr)
        {
            values.emplace_back(*ptr);
        }
    }
    iface->register_property(name, values);
}

template <typename JsonType>
bool SetJsonFromPointer(const std::string &ptrStr, const JsonType &value,
                        nlohmann::json &systemConfiguration)
{
    try
    {
        nlohmann::json::json_pointer ptr(ptrStr);
        nlohmann::json &ref = systemConfiguration[ptr];
        ref = value;
        return true;
    }
    catch (const std::out_of_range)
    {
        return false;
    }
}
// adds simple json types to interface's properties
void populateInterfaceFromJson(nlohmann::json &systemConfiguration,
                               const std::string &jsonPointerPath,
                               sdbusplus::asio::dbus_interface *iface,
                               nlohmann::json &dict,
                               sdbusplus::asio::object_server &objServer,
                               bool setable = false)
{
    for (auto &dictPair : dict.items())
    {
        auto type = dictPair.value().type();
        bool array = false;
        if (dictPair.value().type() == nlohmann::json::value_t::array)
        {
            array = true;
            if (!dictPair.value().size())
            {
                continue;
            }
            type = dictPair.value()[0].type();
            bool isLegal = true;
            for (const auto &arrayItem : dictPair.value())
            {
                if (arrayItem.type() != type)
                {
                    isLegal = false;
                    break;
                }
            }
            if (!isLegal)
            {
                std::cerr << "dbus format error" << dictPair.value() << "\n";
                continue;
            }
            if (type == nlohmann::json::value_t::object)
            {
                continue; // handled elsewhere
            }
        }
        std::string key = jsonPointerPath + "/" + dictPair.key();
        switch (type)
        {
            case (nlohmann::json::value_t::boolean):
            {
                if (array)
                {
                    // todo: array of bool isn't detected correctly by
                    // sdbusplus, change it to numbers
                    addArrayToDbus<uint64_t>(dictPair.key(), dictPair.value(),
                                             iface);
                    break;
                }
                if (setable)
                {
                    iface->register_property(
                        std::string(dictPair.key()),
                        dictPair.value().get<bool>(),
                        [&, key](const bool &newVal, bool &val) {
                            val = newVal;
                            if (!SetJsonFromPointer(key, val,
                                                    systemConfiguration))
                            {
                                std::cerr << "error writing json\n";
                                return -1;
                            }
                            writeJsonFiles(systemConfiguration);
                            return 1;
                        });
                }
                else
                {
                    iface->register_property(std::string(dictPair.key()),
                                             dictPair.value().get<bool>());
                }
                break;
            }
            case (nlohmann::json::value_t::number_integer):
            {
                if (array)
                {
                    addArrayToDbus<int64_t>(dictPair.key(), dictPair.value(),
                                            iface);
                    break;
                }
                if (setable)
                {
                    iface->register_property(
                        std::string(dictPair.key()),
                        dictPair.value().get<int64_t>(),
                        [&, key](const int64_t &newVal, int64_t &val) {
                            val = newVal;
                            if (!SetJsonFromPointer(key, val,
                                                    systemConfiguration))
                            {
                                std::cerr << "error writing json\n";
                                return -1;
                            }
                            writeJsonFiles(systemConfiguration);
                            return 1;
                        });
                }
                else
                {
                    iface->register_property(std::string(dictPair.key()),
                                             dictPair.value().get<int64_t>());
                }
                break;
            }
            case (nlohmann::json::value_t::number_unsigned):
            {
                if (array)
                {
                    addArrayToDbus<uint64_t>(dictPair.key(), dictPair.value(),
                                             iface);
                    break;
                }
                if (setable)
                {
                    iface->register_property(
                        std::string(dictPair.key()),
                        dictPair.value().get<uint64_t>(),
                        [&, key](const uint64_t &newVal, uint64_t &val) {
                            val = newVal;
                            if (!SetJsonFromPointer(key, val,
                                                    systemConfiguration))
                            {
                                std::cerr << "error writing json\n";
                                return -1;
                            }
                            writeJsonFiles(systemConfiguration);
                            return 1;
                        });
                }
                else
                {
                    iface->register_property(std::string(dictPair.key()),
                                             dictPair.value().get<uint64_t>());
                }
                break;
            }
            case (nlohmann::json::value_t::number_float):
            {
                if (array)
                {
                    addArrayToDbus<double>(dictPair.key(), dictPair.value(),
                                           iface);
                    break;
                }
                if (setable)
                {
                    iface->register_property(
                        std::string(dictPair.key()),
                        dictPair.value().get<double>(),
                        [&, key](const double &newVal, double &val) {
                            val = newVal;
                            if (!SetJsonFromPointer(key, val,
                                                    systemConfiguration))
                            {
                                std::cerr << "error writing json\n";
                                return -1;
                            }
                            return 1;
                        });
                }
                else
                {
                    iface->register_property(std::string(dictPair.key()),
                                             dictPair.value().get<double>());
                }
                break;
            }
            case (nlohmann::json::value_t::string):
            {
                if (array)
                {
                    addArrayToDbus<std::string>(dictPair.key(),
                                                dictPair.value(), iface);
                    break;
                }
                if (setable)
                {
                    iface->register_property(
                        std::string(dictPair.key()),
                        dictPair.value().get<std::string>(),
                        [&, key](const std::string &newVal, std::string &val) {
                            val = newVal;
                            if (!SetJsonFromPointer(key, val,
                                                    systemConfiguration))
                            {
                                std::cerr << "error writing json\n";
                                return -1;
                            }
                            writeJsonFiles(systemConfiguration);
                            return 1;
                        });
                }
                else
                {
                    iface->register_property(
                        std::string(dictPair.key()),
                        dictPair.value().get<std::string>());
                }
                break;
            }
        }
    }

    iface->initialize();
}

void postToDbus(const nlohmann::json &newConfiguration,
                nlohmann::json &systemConfiguration,
                sdbusplus::asio::object_server &objServer)

{
    // iterate through boards
    for (auto &boardPair : newConfiguration.items())
    {
        std::string boardKey = boardPair.key();
        std::vector<std::string> path;
        std::string jsonPointerPath = "/" + boardKey;
        // loop through newConfiguration, but use values from system
        // configuration to be able to modify via dbus later
        auto boardValues = systemConfiguration[boardKey];
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
        std::string boardtypeLower = boost::algorithm::to_lower_copy(boardType);

        std::regex_replace(boardKey.begin(), boardKey.begin(), boardKey.end(),
                           ILLEGAL_DBUS_REGEX, "_");
        std::string boardName = "/xyz/openbmc_project/inventory/system/" +
                                boardtypeLower + "/" + boardKey;

        auto inventoryIface = objServer.add_interface(
            boardName, "xyz.openbmc_project.Inventory.Item");
        auto boardIface = objServer.add_interface(
            boardName, "xyz.openbmc_project.Inventory.Item." + boardType);

        populateInterfaceFromJson(systemConfiguration, jsonPointerPath,
                                  boardIface.get(), boardValues, objServer);
        jsonPointerPath += "/";
        // iterate through board properties
        for (auto &boardField : boardValues.items())
        {
            if (boardField.value().type() == nlohmann::json::value_t::object)
            {
                auto iface =
                    objServer.add_interface(boardName, boardField.key());
                populateInterfaceFromJson(
                    systemConfiguration, jsonPointerPath + boardField.key(),
                    iface.get(), boardField.value(), objServer);
            }
        }

        auto exposes = boardValues.find("exposes");
        if (exposes == boardValues.end())
        {
            continue;
        }
        // iterate through exposes
        jsonPointerPath += "exposes/";

        // store the board level pointer so we can modify it on the way down
        std::string jsonPointerPathBoard = jsonPointerPath;
        size_t exposesIndex = -1;
        for (auto &item : *exposes)
        {
            exposesIndex++;
            jsonPointerPath = jsonPointerPathBoard;
            jsonPointerPath += std::to_string(exposesIndex);

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
            auto itemIface = objServer.add_interface(
                boardName + "/" + itemName,
                "xyz.openbmc_project.Configuration." + itemType);

            populateInterfaceFromJson(systemConfiguration, jsonPointerPath,
                                      itemIface.get(), item, objServer);

            for (auto &objectPair : item.items())
            {
                jsonPointerPath = jsonPointerPathBoard +
                                  std::to_string(exposesIndex) + "/" +
                                  objectPair.key();
                if (objectPair.value().type() ==
                    nlohmann::json::value_t::object)
                {
                    auto objectIface = objServer.add_interface(
                        boardName + "/" + itemName,
                        "xyz.openbmc_project.Configuration." + itemType + "." +
                            objectPair.key());

                    populateInterfaceFromJson(
                        systemConfiguration, jsonPointerPath, objectIface.get(),
                        objectPair.value(), objServer);
                }
                else if (objectPair.value().type() ==
                         nlohmann::json::value_t::array)
                {
                    size_t index = 0;
                    if (!objectPair.value().size())
                    {
                        continue;
                    }
                    bool isLegal = true;
                    auto type = objectPair.value()[0].type();
                    if (type != nlohmann::json::value_t::object)
                    {
                        continue;
                    }

                    // verify legal json
                    for (const auto &arrayItem : objectPair.value())
                    {
                        if (arrayItem.type() != type)
                        {
                            isLegal = false;
                            break;
                        }
                    }
                    if (!isLegal)
                    {
                        std::cerr << "dbus format error" << objectPair.value()
                                  << "\n";
                        break;
                    }

                    for (auto &arrayItem : objectPair.value())
                    {
                        // limit what interfaces accept set for saftey
                        bool setable = std::find(SETTABLE_INTERFACES.begin(),
                                                 SETTABLE_INTERFACES.end(),
                                                 objectPair.key()) !=
                                       SETTABLE_INTERFACES.end();

                        auto objectIface = objServer.add_interface(
                            boardName + "/" + itemName,
                            "xyz.openbmc_project.Configuration." + itemType +
                                "." + objectPair.key() +
                                std::to_string(index++));
                        populateInterfaceFromJson(
                            systemConfiguration,
                            jsonPointerPath + "/" + std::to_string(index),
                            objectIface.get(), arrayItem, objServer, setable);
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
    const boost::container::flat_map<std::string, BasicVariantType>
        &foundDevice,
    size_t &foundDeviceIdx)
{
    if (keyPair.value().type() == nlohmann::json::value_t::object)
    {
        for (auto nextLayer = keyPair.value().begin();
             nextLayer != keyPair.value().end(); nextLayer++)
        {
            templateCharReplace(nextLayer, foundDevice, foundDeviceIdx);
        }
        return;
    }
    else if (keyPair.value().type() != nlohmann::json::value_t::string)
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
            bool found = false;
            for (auto &foundDevicePair : foundDevice)
            {
                if (boost::iequals(foundDevicePair.first, templateValue))
                {
                    mapbox::util::apply_visitor(
                        [&](auto &&val) { keyPair.value() = val; },
                        foundDevicePair.second);
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                std::cerr << "could not find symbol " << templateValue << "\n";
            }
        }
    }
}

// reads json files out of the filesystem
bool findJsonFiles(std::list<nlohmann::json> &configurations)
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

struct PerformScan : std::enable_shared_from_this<PerformScan>
{

    PerformScan(nlohmann::json &systemConfiguration,
                std::list<nlohmann::json> &configurations,
                std::function<void(void)> &&callback) :
        _systemConfiguration(systemConfiguration),
        _configurations(configurations), _callback(std::move(callback))
    {
    }
    void run()
    {
        for (auto it = _configurations.begin(); it != _configurations.end();)
        {
            auto findProbe = it->find("probe");
            auto findName = it->find("name");

            nlohmann::json probeCommand;
            // check for poorly formatted fields, probe must be an array
            if (findProbe == it->end())
            {
                std::cerr << "configuration file missing probe:\n " << *it
                          << "\n";
                it = _configurations.erase(it);
                continue;
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
                it = _configurations.erase(it);
                continue;
            }
            std::string name = *findName;

            if (std::find(PASSED_PROBES.begin(), PASSED_PROBES.end(), name) !=
                PASSED_PROBES.end())
            {
                it = _configurations.erase(it);
                continue;
            }
            nlohmann::json *record = &(*it);

            // store reference to this to children to makes sure we don't get
            // destroyed too early
            auto thisRef = shared_from_this();
            auto p = std::make_shared<PerformProbe>(
                probeCommand,
                [&, record, name,
                 thisRef](std::vector<boost::container::flat_map<
                              std::string, BasicVariantType>> &foundDevices) {
                    _passed = true;

                    PASSED_PROBES.push_back(name);
                    size_t foundDeviceIdx = 0;

                    // insert into configuration temporarly to be able to
                    // reference ourselves
                    _systemConfiguration[name] = *record;

                    for (auto &foundDevice : foundDevices)
                    {
                        for (auto keyPair = record->begin();
                             keyPair != record->end(); keyPair++)
                        {
                            templateCharReplace(keyPair, foundDevice,
                                                foundDeviceIdx);
                        }
                        auto findExpose = record->find("exposes");
                        if (findExpose == record->end())
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
                                        std::cerr << "bind_ value must be of "
                                                     "type string "
                                                  << keyPair.key() << "\n";
                                        continue;
                                    }
                                    bool foundBind = false;
                                    std::string bind = keyPair.key().substr(
                                        sizeof("bind_") - 1);

                                    for (auto &configurationPair :
                                         _systemConfiguration.items())
                                    {

                                        auto configListFind =
                                            configurationPair.value().find(
                                                "exposes");

                                        if (configListFind ==
                                                configurationPair.value()
                                                    .end() ||
                                            configListFind->type() !=
                                                nlohmann::json::value_t::array)
                                        {
                                            continue;
                                        }
                                        for (auto &exposedObject :
                                             *configListFind)
                                        {
                                            std::string foundObjectName =
                                                (exposedObject)["name"];
                                            if (boost::iequals(
                                                    foundObjectName,
                                                    keyPair.value()
                                                        .get<std::string>()))
                                            {
                                                exposedObject["status"] =
                                                    "okay";
                                                expose[bind] = exposedObject;

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
                    // overwrite ourselves with cleaned up version
                    _systemConfiguration[name] = *record;
                });
            p->run();
            it++;
        }
    }

    ~PerformScan()
    {
        if (_passed)
        {
            auto nextScan = std::make_shared<PerformScan>(
                _systemConfiguration, _configurations, std::move(_callback));
            nextScan->run();
        }
        else
        {
            _callback();
        }
    }
    nlohmann::json &_systemConfiguration;
    std::list<nlohmann::json> _configurations;
    std::function<void(void)> _callback;
    std::vector<std::shared_ptr<PerformProbe>> _probes;
    bool _passed = false;
};

// main properties changed entry
void propertiesChangedCallback(
    boost::asio::io_service &io,
    std::vector<sdbusplus::bus::match::match> &dbusMatches,
    nlohmann::json &systemConfiguration,
    sdbusplus::asio::object_server &objServer)
{
    static boost::asio::deadline_timer timer(io);
    timer.expires_from_now(boost::posix_time::seconds(1));

    // setup an async wait as we normally get flooded with new requests
    timer.async_wait([&](const boost::system::error_code &ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            // we were cancelled
            return;
        }
        else if (ec)
        {
            std::cerr << "async wait error " << ec << "\n";
            return;
        }

        nlohmann::json oldConfiguration = systemConfiguration;
        DBUS_PROBE_OBJECTS.clear();

        std::list<nlohmann::json> configurations;
        if (!findJsonFiles(configurations))
        {
            std::cerr << "cannot find json files\n";
            return;
        }

        auto perfScan = std::make_shared<PerformScan>(
            systemConfiguration, configurations, [&, oldConfiguration]() {
                nlohmann::json newConfiguration = systemConfiguration;
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
                registerCallbacks(io, dbusMatches, systemConfiguration,
                                  objServer);
                io.post([&, newConfiguration]() {
                    // todo: for now, only add new configurations,
                    // unload to come later unloadOverlays();
                    loadOverlays(newConfiguration);
                    io.post([&]() { writeJsonFiles(systemConfiguration); });
                    io.post([&, newConfiguration]() {
                        postToDbus(newConfiguration, systemConfiguration,
                                   objServer);
                    });
                });
            });
        perfScan->run();
    });
}

void registerCallbacks(boost::asio::io_service &io,
                       std::vector<sdbusplus::bus::match::match> &dbusMatches,
                       nlohmann::json &systemConfiguration,
                       sdbusplus::asio::object_server &objServer)
{
    static boost::container::flat_set<std::string> watchedObjects;

    for (const auto &objectMap : DBUS_PROBE_OBJECTS)
    {
        auto findObject = watchedObjects.find(objectMap.first);
        if (findObject != watchedObjects.end())
        {
            continue;
        }
        std::function<void(sdbusplus::message::message & message)>
            eventHandler =

                [&](sdbusplus::message::message &) {
                    propertiesChangedCallback(io, dbusMatches,
                                              systemConfiguration, objServer);
                };

        sdbusplus::bus::match::match match(
            static_cast<sdbusplus::bus::bus &>(*SYSTEM_BUS),
            "type='signal',member='PropertiesChanged',arg0='" +
                objectMap.first + "'",
            eventHandler);
        dbusMatches.emplace_back(std::move(match));
    }
}

int main(int argc, char **argv)
{
    // setup connection to dbus
    boost::asio::io_service io;
    SYSTEM_BUS = std::make_shared<sdbusplus::asio::connection>(io);
    SYSTEM_BUS->request_name("xyz.openbmc_project.EntityManager");

    sdbusplus::asio::object_server objServer(SYSTEM_BUS);

    std::shared_ptr<sdbusplus::asio::dbus_interface> entityIface =
        objServer.add_interface("/xyz/openbmc_project/EntityManager",
                                "xyz.openbmc_project.EntityManager");

    std::shared_ptr<sdbusplus::asio::dbus_interface> inventoryIface =
        objServer.add_interface("/xyz/openbmc_project/inventory",
                                "xyz.openbmc_project.Inventory.Manager");

    // to keep reference to the match / filter objects so they don't get
    // destroyed
    std::vector<sdbusplus::bus::match::match> dbusMatches;

    nlohmann::json systemConfiguration = nlohmann::json::object();

    inventoryIface->register_method(
        "Notify", [](const boost::container::flat_map<
                      std::string,
                      boost::container::flat_map<std::string, BasicVariantType>>
                         &object) { return; });
    inventoryIface->initialize();

    io.post([&]() {
        unloadAllOverlays();
        propertiesChangedCallback(io, dbusMatches, systemConfiguration,
                                  objServer);
    });

    entityIface->register_method("ReScan", [&]() {
        propertiesChangedCallback(io, dbusMatches, systemConfiguration,
                                  objServer);
    });
    entityIface->initialize();

    io.run();

    return 0;
}
