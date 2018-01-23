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
#include <regex>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/variant/apply_visitor.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <VariantVisitors.hpp>

constexpr const char *OUTPUT_DIR = "/var/configuration/";
constexpr const char *CONFIGURATION_DIR = "/usr/share/configurations";
constexpr const char *TEMPLATE_CHAR = "$";
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
                commandStr = boost::replace_all_copy(commandStr, "'", "");
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
            commandStr = boost::replace_all_copy(commandStr, "'", R"(")");
            commandStr = boost::replace_all_copy(commandStr, R"(\)", R"(\\)");
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

int main(int argc, char **argv)
{
    // find configuration files
    std::vector<fs::path> jsonPaths;
    if (!find_files(fs::path(CONFIGURATION_DIR), R"(.*\.json)", jsonPaths, 0))
    {
        std::cerr << "Unable to find any configuration files in "
                  << CONFIGURATION_DIR << "\n";
        return 1;
    }
    // setup connection to dbus
    boost::asio::io_service io;
    SYSTEM_BUS = std::make_shared<dbus::connection>(io, dbus::bus::system);
    dbus::DbusObjectServer objServer(SYSTEM_BUS);
    SYSTEM_BUS->request_name("xyz.openbmc_project.EntityManager");

    std::vector<nlohmann::json> configurations;
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

    // keep looping as long as at least 1 new probe passed, removing
    // configurations from the memory store once the probe passes
    bool probePassed = true;
    nlohmann::json systemConfiguration;
    while (probePassed)
    {
        probePassed = false;
        for (auto it = configurations.begin(); it != configurations.end();)
        {
            bool eraseConfig = false;
            auto findProbe = (*it).find("probe");
            auto findName = (*it).find("name");

            // check for poorly formatted fields
            if (findProbe == (*it).end())
            {
                std::cerr << "configuration file missing probe:\n " << *it
                          << "\n";
                eraseConfig = true;
            }
            if (findName == (*it).end())
            {
                std::cerr << "configuration file missing name:\n " << *it
                          << "\n";
                eraseConfig = true;
            }

            nlohmann::json probeCommand;
            if ((*findProbe).type() != nlohmann::json::value_t::array)
            {
                probeCommand = nlohmann::json::array();
                probeCommand.push_back(*findProbe);
            }
            else
            {
                probeCommand = *findProbe;
            }
            std::vector<
                boost::container::flat_map<std::string, dbus::dbus_variant>>
                foundDevices;
            if (probe(probeCommand, foundDevices))
            {
                eraseConfig = true;
                probePassed = true;
                PASSED_PROBES.push_back(*findName);

                size_t foundDeviceIdx = 0;

                for (auto &foundDevice : foundDevices)
                {
                    auto findExpose = (*it).find("exposes");
                    if (findExpose == (*it).end())
                    {
                        std::cerr
                            << "Warning, configuration file missing exposes"
                            << *it << "\n";
                        continue;
                    }
                    for (auto &expose : *findExpose)
                    {
                        for (auto keyPair = expose.begin();
                             keyPair != expose.end(); keyPair++)
                        {
                            // fill in template characters with devices
                            // found
                            if (keyPair.value().type() ==
                                nlohmann::json::value_t::string)
                            {
                                std::string value = keyPair.value();
                                if (value.find(TEMPLATE_CHAR) !=
                                    std::string::npos)
                                {
                                    std::string templateValue = value;

                                    templateValue.erase(
                                        0, 1); // remove template character

                                    // special case index
                                    if ("index" == templateValue)
                                    {
                                        keyPair.value() = foundDeviceIdx;
                                    }
                                    else
                                    {
                                        std::string subsitute;
                                        for (auto &foundDevicePair :
                                             foundDevice)
                                        {
                                            if (boost::iequals(
                                                    foundDevicePair.first,
                                                    templateValue))
                                            {
                                                // convert value to string
                                                // respresentation
                                                subsitute =
                                                    boost::apply_visitor(
                                                        [](const auto &x) {
                                                            return boost::
                                                                lexical_cast<
                                                                    std::
                                                                        string>(
                                                                    x);
                                                        },
                                                        foundDevicePair.second);
                                                break;
                                            }
                                        }
                                        if (!subsitute.size())
                                        {
                                            std::cerr << "could not find "
                                                      << templateValue
                                                      << " in device "
                                                      << expose["name"] << "\n";
                                        }
                                        else
                                        {
                                            keyPair.value() = subsitute;
                                        }
                                    }
                                }

                                // special case bind
                                if (boost::starts_with(keyPair.key(), "bind_"))
                                {
                                    bool foundBind = false;
                                    std::string bind = keyPair.key().substr(
                                        sizeof("bind_") - 1);
                                    for (auto &configuration :
                                         systemConfiguration)
                                    {
                                        auto &configList =
                                            configuration["exposes"];
                                        for (auto exposedObjectIt =
                                                 configList.begin();
                                             exposedObjectIt !=
                                             configList.end();)
                                        {
                                            std::string foundObjectName =
                                                (*exposedObjectIt)["name"];
                                            if (boost::iequals(foundObjectName,
                                                               value))
                                            {
                                                (*exposedObjectIt)["status"] =
                                                    "okay"; // todo: is this the
                                                            // right spot?
                                                expose[bind] =
                                                    (*exposedObjectIt);
                                                foundBind = true;
                                                exposedObjectIt =
                                                    configList.erase(
                                                        exposedObjectIt);
                                                break;
                                            }
                                            else
                                            {
                                                exposedObjectIt++;
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
                                                  << value << "\n";
                                        return 1;
                                    }
                                }
                            }
                        }
                    }
                    systemConfiguration.push_back(*it);
                    foundDeviceIdx++;
                }
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
    // below here is temporary, to be added to dbus
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

    return 0;
}