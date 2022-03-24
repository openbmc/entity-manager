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
/// \file EntityManager.cpp

#include "EntityManager.hpp"

#include "Overlay.hpp"
#include "Utils.hpp"
#include "VariantVisitors.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/range/iterator_range.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <variant>
constexpr const char* hostConfigurationDirectory = SYSCONF_DIR "configurations";
constexpr const char* configurationDirectory = PACKAGE_DIR "configurations";
constexpr const char* schemaDirectory = PACKAGE_DIR "configurations/schemas";
constexpr const char* tempConfigDir = "/tmp/configuration/";
constexpr const char* lastConfiguration = "/tmp/configuration/last.json";
constexpr const char* currentConfiguration = "/var/configuration/system.json";
constexpr const char* globalSchema = "global.json";

const boost::container::flat_map<const char*, probe_type_codes, CmpStr>
    probeTypes{{{"FALSE", probe_type_codes::FALSE_T},
                {"TRUE", probe_type_codes::TRUE_T},
                {"AND", probe_type_codes::AND},
                {"OR", probe_type_codes::OR},
                {"FOUND", probe_type_codes::FOUND},
                {"MATCH_ONE", probe_type_codes::MATCH_ONE}}};

static constexpr std::array<const char*, 6> settableInterfaces = {
    "FanProfile", "Pid", "Pid.Zone", "Stepwise", "Thresholds", "Polling"};
using JsonVariantType =
    std::variant<std::vector<std::string>, std::vector<double>, std::string,
                 int64_t, uint64_t, double, int32_t, uint32_t, int16_t,
                 uint16_t, uint8_t, bool>;

using ManagedObjectType = boost::container::flat_map<
    sdbusplus::message::object_path,
    boost::container::flat_map<
        std::string,
        boost::container::flat_map<std::string, BasicVariantType>>>;

// store reference to all interfaces so we can destroy them later
boost::container::flat_map<
    std::string, std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>>
    inventory;

// todo: pass this through nicer
std::shared_ptr<sdbusplus::asio::connection> systemBus;
nlohmann::json lastJson;

boost::asio::io_context io;

const std::regex illegalDbusPathRegex("[^A-Za-z0-9_.]");
const std::regex illegalDbusMemberRegex("[^A-Za-z0-9_]");

FoundProbeTypeT findProbeType(const std::string& probe)
{
    boost::container::flat_map<const char*, probe_type_codes,
                               CmpStr>::const_iterator probeType;
    for (probeType = probeTypes.begin(); probeType != probeTypes.end();
         ++probeType)
    {
        if (probe.find(probeType->first) != std::string::npos)
        {
            return probeType;
        }
    }

    return std::nullopt;
}

static std::shared_ptr<sdbusplus::asio::dbus_interface>
    createInterface(sdbusplus::asio::object_server& objServer,
                    const std::string& path, const std::string& interface,
                    const std::string& parent, bool checkNull = false)
{
    // on first add we have no reason to check for null before add, as there
    // won't be any. For dynamically added interfaces, we check for null so that
    // a constant delete/add will not create a memory leak

    auto ptr = objServer.add_interface(path, interface);
    auto& dataVector = inventory[parent];
    if (checkNull)
    {
        auto it = std::find_if(dataVector.begin(), dataVector.end(),
                               [](const auto& p) { return p.expired(); });
        if (it != dataVector.end())
        {
            *it = ptr;
            return ptr;
        }
    }
    dataVector.emplace_back(ptr);
    return ptr;
}

// writes output files to persist data
bool writeJsonFiles(const nlohmann::json& systemConfiguration)
{
    std::filesystem::create_directory(configurationOutDir);
    std::ofstream output(currentConfiguration);
    if (!output.good())
    {
        return false;
    }
    output << systemConfiguration.dump(4);
    output.close();
    return true;
}

template <typename JsonType>
bool setJsonFromPointer(const std::string& ptrStr, const JsonType& value,
                        nlohmann::json& systemConfiguration)
{
    try
    {
        nlohmann::json::json_pointer ptr(ptrStr);
        nlohmann::json& ref = systemConfiguration[ptr];
        ref = value;
        return true;
    }
    catch (const std::out_of_range&)
    {
        return false;
    }
}

// template function to add array as dbus property
template <typename PropertyType>
void addArrayToDbus(const std::string& name, const nlohmann::json& array,
                    sdbusplus::asio::dbus_interface* iface,
                    sdbusplus::asio::PropertyPermission permission,
                    nlohmann::json& systemConfiguration,
                    const std::string& jsonPointerString)
{
    std::vector<PropertyType> values;
    for (const auto& property : array)
    {
        auto ptr = property.get_ptr<const PropertyType*>();
        if (ptr != nullptr)
        {
            values.emplace_back(*ptr);
        }
    }

    if (permission == sdbusplus::asio::PropertyPermission::readOnly)
    {
        iface->register_property(name, values);
    }
    else
    {
        iface->register_property(
            name, values,
            [&systemConfiguration,
             jsonPointerString{std::string(jsonPointerString)}](
                const std::vector<PropertyType>& newVal,
                std::vector<PropertyType>& val) {
                val = newVal;
                if (!setJsonFromPointer(jsonPointerString, val,
                                        systemConfiguration))
                {
                    std::cerr << "error setting json field\n";
                    return -1;
                }
                if (!writeJsonFiles(systemConfiguration))
                {
                    std::cerr << "error setting json file\n";
                    return -1;
                }
                return 1;
            });
    }
}

template <typename PropertyType>
void addProperty(const std::string& propertyName, const PropertyType& value,
                 sdbusplus::asio::dbus_interface* iface,
                 nlohmann::json& systemConfiguration,
                 const std::string& jsonPointerString,
                 sdbusplus::asio::PropertyPermission permission)
{
    if (permission == sdbusplus::asio::PropertyPermission::readOnly)
    {
        iface->register_property(propertyName, value);
        return;
    }
    iface->register_property(
        propertyName, value,
        [&systemConfiguration,
         jsonPointerString{std::string(jsonPointerString)}](
            const PropertyType& newVal, PropertyType& val) {
            val = newVal;
            if (!setJsonFromPointer(jsonPointerString, val,
                                    systemConfiguration))
            {
                std::cerr << "error setting json field\n";
                return -1;
            }
            if (!writeJsonFiles(systemConfiguration))
            {
                std::cerr << "error setting json file\n";
                return -1;
            }
            return 1;
        });
}

void createDeleteObjectMethod(
    const std::string& jsonPointerPath,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    sdbusplus::asio::object_server& objServer,
    nlohmann::json& systemConfiguration)
{
    std::weak_ptr<sdbusplus::asio::dbus_interface> interface = iface;
    iface->register_method(
        "Delete", [&objServer, &systemConfiguration, interface,
                   jsonPointerPath{std::string(jsonPointerPath)}]() {
            std::shared_ptr<sdbusplus::asio::dbus_interface> dbusInterface =
                interface.lock();
            if (!dbusInterface)
            {
                // this technically can't happen as the pointer is pointing to
                // us
                throw DBusInternalError();
            }
            nlohmann::json::json_pointer ptr(jsonPointerPath);
            systemConfiguration[ptr] = nullptr;

            // todo(james): dig through sdbusplus to find out why we can't
            // delete it in a method call
            io.post([&objServer, dbusInterface]() mutable {
                objServer.remove_interface(dbusInterface);
            });

            if (!writeJsonFiles(systemConfiguration))
            {
                std::cerr << "error setting json file\n";
                throw DBusInternalError();
            }
        });
}

// adds simple json types to interface's properties
void populateInterfaceFromJson(
    nlohmann::json& systemConfiguration, const std::string& jsonPointerPath,
    std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    nlohmann::json& dict, sdbusplus::asio::object_server& objServer,
    sdbusplus::asio::PropertyPermission permission =
        sdbusplus::asio::PropertyPermission::readOnly)
{
    for (auto& [key, value] : dict.items())
    {
        auto type = value.type();
        bool array = false;
        if (value.type() == nlohmann::json::value_t::array)
        {
            array = true;
            if (!value.size())
            {
                continue;
            }
            type = value[0].type();
            bool isLegal = true;
            for (const auto& arrayItem : value)
            {
                if (arrayItem.type() != type)
                {
                    isLegal = false;
                    break;
                }
            }
            if (!isLegal)
            {
                std::cerr << "dbus format error" << value << "\n";
                continue;
            }
        }
        if (type == nlohmann::json::value_t::object)
        {
            continue; // handled elsewhere
        }

        std::string path = jsonPointerPath;
        path.append("/").append(key);
        if (permission == sdbusplus::asio::PropertyPermission::readWrite)
        {
            // all setable numbers are doubles as it is difficult to always
            // create a configuration file with all whole numbers as decimals
            // i.e. 1.0
            if (array)
            {
                if (value[0].is_number())
                {
                    type = nlohmann::json::value_t::number_float;
                }
            }
            else if (value.is_number())
            {
                type = nlohmann::json::value_t::number_float;
            }
        }

        switch (type)
        {
            case (nlohmann::json::value_t::boolean):
            {
                if (array)
                {
                    // todo: array of bool isn't detected correctly by
                    // sdbusplus, change it to numbers
                    addArrayToDbus<uint64_t>(key, value, iface.get(),
                                             permission, systemConfiguration,
                                             path);
                }

                else
                {
                    addProperty(key, value.get<bool>(), iface.get(),
                                systemConfiguration, path, permission);
                }
                break;
            }
            case (nlohmann::json::value_t::number_integer):
            {
                if (array)
                {
                    addArrayToDbus<int64_t>(key, value, iface.get(), permission,
                                            systemConfiguration, path);
                }
                else
                {
                    addProperty(key, value.get<int64_t>(), iface.get(),
                                systemConfiguration, path,
                                sdbusplus::asio::PropertyPermission::readOnly);
                }
                break;
            }
            case (nlohmann::json::value_t::number_unsigned):
            {
                if (array)
                {
                    addArrayToDbus<uint64_t>(key, value, iface.get(),
                                             permission, systemConfiguration,
                                             path);
                }
                else
                {
                    addProperty(key, value.get<uint64_t>(), iface.get(),
                                systemConfiguration, path,
                                sdbusplus::asio::PropertyPermission::readOnly);
                }
                break;
            }
            case (nlohmann::json::value_t::number_float):
            {
                if (array)
                {
                    addArrayToDbus<double>(key, value, iface.get(), permission,
                                           systemConfiguration, path);
                }

                else
                {
                    addProperty(key, value.get<double>(), iface.get(),
                                systemConfiguration, path, permission);
                }
                break;
            }
            case (nlohmann::json::value_t::string):
            {
                if (array)
                {
                    addArrayToDbus<std::string>(key, value, iface.get(),
                                                permission, systemConfiguration,
                                                path);
                }
                else
                {
                    addProperty(key, value.get<std::string>(), iface.get(),
                                systemConfiguration, path, permission);
                }
                break;
            }
            default:
            {
                std::cerr << "Unexpected json type in system configuration "
                          << key << ": " << value.type_name() << "\n";
                break;
            }
        }
    }
    if (permission == sdbusplus::asio::PropertyPermission::readWrite)
    {
        createDeleteObjectMethod(jsonPointerPath, iface, objServer,
                                 systemConfiguration);
    }
    iface->initialize();
}

sdbusplus::asio::PropertyPermission getPermission(const std::string& interface)
{
    return std::find(settableInterfaces.begin(), settableInterfaces.end(),
                     interface) != settableInterfaces.end()
               ? sdbusplus::asio::PropertyPermission::readWrite
               : sdbusplus::asio::PropertyPermission::readOnly;
}

void createAddObjectMethod(const std::string& jsonPointerPath,
                           const std::string& path,
                           nlohmann::json& systemConfiguration,
                           sdbusplus::asio::object_server& objServer,
                           const std::string& board)
{
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface = createInterface(
        objServer, path, "xyz.openbmc_project.AddObject", board);

    iface->register_method(
        "AddObject",
        [&systemConfiguration, &objServer,
         jsonPointerPath{std::string(jsonPointerPath)}, path{std::string(path)},
         board](const boost::container::flat_map<std::string, JsonVariantType>&
                    data) {
            nlohmann::json::json_pointer ptr(jsonPointerPath);
            nlohmann::json& base = systemConfiguration[ptr];
            auto findExposes = base.find("Exposes");

            if (findExposes == base.end())
            {
                throw std::invalid_argument("Entity must have children.");
            }

            // this will throw invalid-argument to sdbusplus if invalid json
            nlohmann::json newData{};
            for (const auto& item : data)
            {
                nlohmann::json& newJson = newData[item.first];
                std::visit([&newJson](auto&& val) { newJson = std::move(val); },
                           item.second);
            }

            auto findName = newData.find("Name");
            auto findType = newData.find("Type");
            if (findName == newData.end() || findType == newData.end())
            {
                throw std::invalid_argument("AddObject missing Name or Type");
            }
            const std::string* type = findType->get_ptr<const std::string*>();
            const std::string* name = findName->get_ptr<const std::string*>();
            if (type == nullptr || name == nullptr)
            {
                throw std::invalid_argument("Type and Name must be a string.");
            }

            bool foundNull = false;
            size_t lastIndex = 0;
            // we add in the "exposes"
            for (const auto& expose : *findExposes)
            {
                if (expose.is_null())
                {
                    foundNull = true;
                    continue;
                }

                if (expose["Name"] == *name && expose["Type"] == *type)
                {
                    throw std::invalid_argument(
                        "Field already in JSON, not adding");
                }

                if (foundNull)
                {
                    continue;
                }

                lastIndex++;
            }

            std::ifstream schemaFile(std::string(schemaDirectory) + "/" +
                                     *type + ".json");
            // todo(james) we might want to also make a list of 'can add'
            // interfaces but for now I think the assumption if there is a
            // schema avaliable that it is allowed to update is fine
            if (!schemaFile.good())
            {
                throw std::invalid_argument(
                    "No schema avaliable, cannot validate.");
            }
            nlohmann::json schema =
                nlohmann::json::parse(schemaFile, nullptr, false);
            if (schema.is_discarded())
            {
                std::cerr << "Schema not legal" << *type << ".json\n";
                throw DBusInternalError();
            }
            if (!validateJson(schema, newData))
            {
                throw std::invalid_argument("Data does not match schema");
            }
            if (foundNull)
            {
                findExposes->at(lastIndex) = newData;
            }
            else
            {
                findExposes->push_back(newData);
            }
            if (!writeJsonFiles(systemConfiguration))
            {
                std::cerr << "Error writing json files\n";
                throw DBusInternalError();
            }
            std::string dbusName = *name;

            std::regex_replace(dbusName.begin(), dbusName.begin(),
                               dbusName.end(), illegalDbusMemberRegex, "_");

            std::shared_ptr<sdbusplus::asio::dbus_interface> interface =
                createInterface(objServer, path + "/" + dbusName,
                                "xyz.openbmc_project.Configuration." + *type,
                                board, true);
            // permission is read-write, as since we just created it, must be
            // runtime modifiable
            populateInterfaceFromJson(
                systemConfiguration,
                jsonPointerPath + "/Exposes/" + std::to_string(lastIndex),
                interface, newData, objServer,
                sdbusplus::asio::PropertyPermission::readWrite);
        });
    iface->initialize();
}

void postToDbus(const nlohmann::json& newConfiguration,
                nlohmann::json& systemConfiguration,
                sdbusplus::asio::object_server& objServer)

{
    // iterate through boards
    for (auto& [boardId, boardConfig] : newConfiguration.items())
    {
        std::string boardKey = boardConfig["Name"];
        std::string boardKeyOrig = boardConfig["Name"];
        std::string jsonPointerPath = "/" + boardId;
        // loop through newConfiguration, but use values from system
        // configuration to be able to modify via dbus later
        auto boardValues = systemConfiguration[boardId];
        auto findBoardType = boardValues.find("Type");
        std::string boardType;
        if (findBoardType != boardValues.end() &&
            findBoardType->type() == nlohmann::json::value_t::string)
        {
            boardType = findBoardType->get<std::string>();
            std::regex_replace(boardType.begin(), boardType.begin(),
                               boardType.end(), illegalDbusMemberRegex, "_");
        }
        else
        {
            std::cerr << "Unable to find type for " << boardKey
                      << " reverting to Chassis.\n";
            boardType = "Chassis";
        }
        std::string boardtypeLower = boost::algorithm::to_lower_copy(boardType);

        std::regex_replace(boardKey.begin(), boardKey.begin(), boardKey.end(),
                           illegalDbusMemberRegex, "_");
        std::string boardName = "/xyz/openbmc_project/inventory/system/";
        boardName += boardtypeLower;
        boardName += "/";
        boardName += boardKey;

        std::shared_ptr<sdbusplus::asio::dbus_interface> inventoryIface =
            createInterface(objServer, boardName,
                            "xyz.openbmc_project.Inventory.Item", boardKey);

        std::shared_ptr<sdbusplus::asio::dbus_interface> boardIface =
            createInterface(objServer, boardName,
                            "xyz.openbmc_project.Inventory.Item." + boardType,
                            boardKeyOrig);

        createAddObjectMethod(jsonPointerPath, boardName, systemConfiguration,
                              objServer, boardKeyOrig);

        populateInterfaceFromJson(systemConfiguration, jsonPointerPath,
                                  boardIface, boardValues, objServer);
        jsonPointerPath += "/";
        // iterate through board properties
        for (auto& [propName, propValue] : boardValues.items())
        {
            if (propValue.type() == nlohmann::json::value_t::object)
            {
                std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
                    createInterface(objServer, boardName, propName,
                                    boardKeyOrig);

                populateInterfaceFromJson(systemConfiguration,
                                          jsonPointerPath + propName, iface,
                                          propValue, objServer);
            }
        }

        auto exposes = boardValues.find("Exposes");
        if (exposes == boardValues.end())
        {
            continue;
        }
        // iterate through exposes
        jsonPointerPath += "Exposes/";

        // store the board level pointer so we can modify it on the way down
        std::string jsonPointerPathBoard = jsonPointerPath;
        size_t exposesIndex = -1;
        for (auto& item : *exposes)
        {
            exposesIndex++;
            jsonPointerPath = jsonPointerPathBoard;
            jsonPointerPath += std::to_string(exposesIndex);

            auto findName = item.find("Name");
            if (findName == item.end())
            {
                std::cerr << "cannot find name in field " << item << "\n";
                continue;
            }
            auto findStatus = item.find("Status");
            // if status is not found it is assumed to be status = 'okay'
            if (findStatus != item.end())
            {
                if (*findStatus == "disabled")
                {
                    continue;
                }
            }
            auto findType = item.find("Type");
            std::string itemType;
            if (findType != item.end())
            {
                itemType = findType->get<std::string>();
                std::regex_replace(itemType.begin(), itemType.begin(),
                                   itemType.end(), illegalDbusPathRegex, "_");
            }
            else
            {
                itemType = "unknown";
            }
            std::string itemName = findName->get<std::string>();
            std::regex_replace(itemName.begin(), itemName.begin(),
                               itemName.end(), illegalDbusMemberRegex, "_");
            std::string ifacePath = boardName;
            ifacePath += "/";
            ifacePath += itemName;

            std::shared_ptr<sdbusplus::asio::dbus_interface> itemIface =
                createInterface(objServer, ifacePath,
                                "xyz.openbmc_project.Configuration." + itemType,
                                boardKeyOrig);

            populateInterfaceFromJson(systemConfiguration, jsonPointerPath,
                                      itemIface, item, objServer,
                                      getPermission(itemType));

            for (auto& [name, config] : item.items())
            {
                jsonPointerPath = jsonPointerPathBoard;
                jsonPointerPath.append(std::to_string(exposesIndex))
                    .append("/")
                    .append(name);
                if (config.type() == nlohmann::json::value_t::object)
                {
                    std::string ifaceName =
                        "xyz.openbmc_project.Configuration.";
                    ifaceName.append(itemType).append(".").append(name);

                    std::shared_ptr<sdbusplus::asio::dbus_interface>
                        objectIface = createInterface(objServer, ifacePath,
                                                      ifaceName, boardKeyOrig);

                    populateInterfaceFromJson(
                        systemConfiguration, jsonPointerPath, objectIface,
                        config, objServer, getPermission(name));
                }
                else if (config.type() == nlohmann::json::value_t::array)
                {
                    size_t index = 0;
                    if (!config.size())
                    {
                        continue;
                    }
                    bool isLegal = true;
                    auto type = config[0].type();
                    if (type != nlohmann::json::value_t::object)
                    {
                        continue;
                    }

                    // verify legal json
                    for (const auto& arrayItem : config)
                    {
                        if (arrayItem.type() != type)
                        {
                            isLegal = false;
                            break;
                        }
                    }
                    if (!isLegal)
                    {
                        std::cerr << "dbus format error" << config << "\n";
                        break;
                    }

                    for (auto& arrayItem : config)
                    {
                        std::string ifaceName =
                            "xyz.openbmc_project.Configuration.";
                        ifaceName.append(itemType).append(".").append(name);
                        ifaceName.append(std::to_string(index));

                        std::shared_ptr<sdbusplus::asio::dbus_interface>
                            objectIface = createInterface(
                                objServer, ifacePath, ifaceName, boardKeyOrig);

                        populateInterfaceFromJson(
                            systemConfiguration,
                            jsonPointerPath + "/" + std::to_string(index),
                            objectIface, arrayItem, objServer,
                            getPermission(name));
                        index++;
                    }
                }
            }
        }
    }
}

// reads json files out of the filesystem
bool findJsonFiles(std::list<nlohmann::json>& configurations)
{
    // find configuration files
    std::vector<std::filesystem::path> jsonPaths;
    if (!findFiles(
            std::vector<std::filesystem::path>{configurationDirectory,
                                               hostConfigurationDirectory},
            R"(.*\.json)", jsonPaths))
    {
        std::cerr << "Unable to find any configuration files in "
                  << configurationDirectory << "\n";
        return false;
    }

    std::ifstream schemaStream(std::string(schemaDirectory) + "/" +
                               globalSchema);
    if (!schemaStream.good())
    {
        std::cerr
            << "Cannot open schema file,  cannot validate JSON, exiting\n\n";
        std::exit(EXIT_FAILURE);
        return false;
    }
    nlohmann::json schema = nlohmann::json::parse(schemaStream, nullptr, false);
    if (schema.is_discarded())
    {
        std::cerr
            << "Illegal schema file detected, cannot validate JSON, exiting\n";
        std::exit(EXIT_FAILURE);
        return false;
    }

    for (auto& jsonPath : jsonPaths)
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
        /*
         * todo(james): reenable this once less things are in flight
         *
        if (!validateJson(schema, data))
        {
            std::cerr << "Error validating " << jsonPath.string() << "\n";
            continue;
        }
        */

        if (data.type() == nlohmann::json::value_t::array)
        {
            for (auto& d : data)
            {
                configurations.emplace_back(d);
            }
        }
        else
        {
            configurations.emplace_back(data);
        }
    }
    return true;
}

static bool deviceRequiresPowerOn(const nlohmann::json& entity)
{
    auto powerState = entity.find("PowerState");
    if (powerState == entity.end())
    {
        return false;
    }

    auto ptr = powerState->get_ptr<const std::string*>();
    if (!ptr)
    {
        return false;
    }

    return *ptr == "On" || *ptr == "BiosPost";
}

static void pruneDevice(const nlohmann::json& systemConfiguration,
                        const bool powerOff, const bool scannedPowerOff,
                        const std::string& name, const nlohmann::json& device)
{
    if (systemConfiguration.contains(name))
    {
        return;
    }

    if (deviceRequiresPowerOn(device) && (powerOff || scannedPowerOff))
    {
        return;
    }

    logDeviceRemoved(device);
}

void startRemovedTimer(boost::asio::steady_timer& timer,
                       nlohmann::json& systemConfiguration)
{
    static bool scannedPowerOff = false;
    static bool scannedPowerOn = false;

    if (systemConfiguration.empty() || lastJson.empty())
    {
        return; // not ready yet
    }
    if (scannedPowerOn)
    {
        return;
    }

    if (!isPowerOn() && scannedPowerOff)
    {
        return;
    }

    timer.expires_after(std::chrono::seconds(10));
    timer.async_wait(
        [&systemConfiguration](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted)
            {
                return;
            }

            bool powerOff = !isPowerOn();
            for (const auto& [name, device] : lastJson.items())
            {
                pruneDevice(systemConfiguration, powerOff, scannedPowerOff,
                            name, device);
            }

            scannedPowerOff = true;
            if (!powerOff)
            {
                scannedPowerOn = true;
            }
        });
}

static std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>&
    getDeviceInterfaces(const nlohmann::json& device)
{
    return inventory[device["Name"].get<std::string>()];
}

static void pruneConfiguration(nlohmann::json& systemConfiguration,
                               sdbusplus::asio::object_server& objServer,
                               bool powerOff, const std::string& name,
                               const nlohmann::json& device)
{
    if (powerOff && deviceRequiresPowerOn(device))
    {
        // power not on yet, don't know if it's there or not
        return;
    }

    auto& ifaces = getDeviceInterfaces(device);
    for (auto& iface : ifaces)
    {
        auto sharedPtr = iface.lock();
        if (!!sharedPtr)
        {
            objServer.remove_interface(sharedPtr);
        }
    }

    ifaces.clear();
    systemConfiguration.erase(name);
    logDeviceRemoved(device);
}

// main properties changed entry
void propertiesChangedCallback(nlohmann::json& systemConfiguration,
                               sdbusplus::asio::object_server& objServer)
{
    static bool inProgress = false;
    static boost::asio::steady_timer timer(io);
    static size_t instance = 0;
    instance++;
    size_t count = instance;

    timer.expires_after(std::chrono::seconds(5));

    // setup an async wait as we normally get flooded with new requests
    timer.async_wait([&systemConfiguration, &objServer,
                      count](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            // we were cancelled
            return;
        }
        if (ec)
        {
            std::cerr << "async wait error " << ec << "\n";
            return;
        }

        if (inProgress)
        {
            propertiesChangedCallback(systemConfiguration, objServer);
            return;
        }
        inProgress = true;

        nlohmann::json oldConfiguration = systemConfiguration;
        auto missingConfigurations = std::make_shared<nlohmann::json>();
        *missingConfigurations = systemConfiguration;

        std::list<nlohmann::json> configurations;
        if (!findJsonFiles(configurations))
        {
            std::cerr << "cannot find json files\n";
            inProgress = false;
            return;
        }

        auto perfScan = std::make_shared<PerformScan>(
            systemConfiguration, *missingConfigurations, configurations,
            objServer,
            [&systemConfiguration, &objServer, count, oldConfiguration,
             missingConfigurations]() {
                // this is something that since ac has been applied to the bmc
                // we saw, and we no longer see it
                bool powerOff = !isPowerOn();
                for (const auto& [name, device] :
                     missingConfigurations->items())
                {
                    pruneConfiguration(systemConfiguration, objServer, powerOff,
                                       name, device);
                }

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
                for (const auto& [_, device] : newConfiguration.items())
                {
                    logDeviceAdded(device);
                }

                inProgress = false;

                io.post([count, newConfiguration, &systemConfiguration,
                         &objServer]() {
                    loadOverlays(newConfiguration);

                    io.post([&systemConfiguration]() {
                        if (!writeJsonFiles(systemConfiguration))
                        {
                            std::cerr << "Error writing json files\n";
                        }
                    });
                    io.post([count, newConfiguration, &systemConfiguration,
                             &objServer]() {
                        postToDbus(newConfiguration, systemConfiguration,
                                   objServer);
                        if (count == instance)
                        {
                            startRemovedTimer(timer, systemConfiguration);
                        }
                    });
                });
            });
        perfScan->run();
    });
}

int main()
{
    // setup connection to dbus
    systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    systemBus->request_name("xyz.openbmc_project.EntityManager");

    sdbusplus::asio::object_server objServer(systemBus);

    std::shared_ptr<sdbusplus::asio::dbus_interface> entityIface =
        objServer.add_interface("/xyz/openbmc_project/EntityManager",
                                "xyz.openbmc_project.EntityManager");

    // to keep reference to the match / filter objects so they don't get
    // destroyed

    nlohmann::json systemConfiguration = nlohmann::json::object();

    // We need a poke from DBus for static providers that create all their
    // objects prior to claiming a well-known name, and thus don't emit any
    // org.freedesktop.DBus.Properties signals.  Similarly if a process exits
    // for any reason, expected or otherwise, we'll need a poke to remove
    // entities from DBus.
    sdbusplus::bus::match::match nameOwnerChangedMatch(
        static_cast<sdbusplus::bus::bus&>(*systemBus),
        sdbusplus::bus::match::rules::nameOwnerChanged(),
        [&](sdbusplus::message::message&) {
            propertiesChangedCallback(systemConfiguration, objServer);
        });
    // We also need a poke from DBus when new interfaces are created or
    // destroyed.
    sdbusplus::bus::match::match interfacesAddedMatch(
        static_cast<sdbusplus::bus::bus&>(*systemBus),
        sdbusplus::bus::match::rules::interfacesAdded(),
        [&](sdbusplus::message::message&) {
            propertiesChangedCallback(systemConfiguration, objServer);
        });
    sdbusplus::bus::match::match interfacesRemovedMatch(
        static_cast<sdbusplus::bus::bus&>(*systemBus),
        sdbusplus::bus::match::rules::interfacesRemoved(),
        [&](sdbusplus::message::message&) {
            propertiesChangedCallback(systemConfiguration, objServer);
        });

    io.post(
        [&]() { propertiesChangedCallback(systemConfiguration, objServer); });

    entityIface->register_method("ReScan", [&]() {
        propertiesChangedCallback(systemConfiguration, objServer);
    });
    entityIface->initialize();

    if (fwVersionIsSame())
    {
        if (std::filesystem::is_regular_file(currentConfiguration))
        {
            // this file could just be deleted, but it's nice for debug
            std::filesystem::create_directory(tempConfigDir);
            std::filesystem::remove(lastConfiguration);
            std::filesystem::copy(currentConfiguration, lastConfiguration);
            std::filesystem::remove(currentConfiguration);

            std::ifstream jsonStream(lastConfiguration);
            if (jsonStream.good())
            {
                auto data = nlohmann::json::parse(jsonStream, nullptr, false);
                if (data.is_discarded())
                {
                    std::cerr << "syntax error in " << lastConfiguration
                              << "\n";
                }
                else
                {
                    lastJson = std::move(data);
                }
            }
            else
            {
                std::cerr << "unable to open " << lastConfiguration << "\n";
            }
        }
    }
    else
    {
        // not an error, just logging at this level to make it in the journal
        std::cerr << "Clearing previous configuration\n";
        std::filesystem::remove(currentConfiguration);
    }

    // some boards only show up after power is on, we want to not say they are
    // removed until the same state happens
    setupPowerMatch(systemBus);

    io.run();

    return 0;
}
