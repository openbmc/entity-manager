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
/// \file entity_manager.cpp

#include "entity_manager.hpp"

#include "configuration.hpp"
#include "overlay.hpp"
#include "perform_scan.hpp"
#include "topology.hpp"
#include "utils.hpp"
#include "variant_visitors.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
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
#include <functional>
#include <iostream>
#include <map>
#include <regex>
#include <variant>
constexpr const char* tempConfigDir = "/tmp/configuration/";
constexpr const char* lastConfiguration = "/tmp/configuration/last.json";

static constexpr std::array<const char*, 6> settableInterfaces = {
    "FanProfile", "Pid", "Pid.Zone", "Stepwise", "Thresholds", "Polling"};
using JsonVariantType =
    std::variant<std::vector<std::string>, std::vector<double>, std::string,
                 int64_t, uint64_t, double, int32_t, uint32_t, int16_t,
                 uint16_t, uint8_t, bool>;

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
// store reference to all interfaces so we can destroy them later
boost::container::flat_map<
    std::string, std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>>
    inventory;

// todo: pass this through nicer
std::shared_ptr<sdbusplus::asio::connection> systemBus;
nlohmann::json lastJson;
Topology topology;

boost::asio::io_context io;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

const std::regex illegalDbusPathRegex("[^A-Za-z0-9_.]");
const std::regex illegalDbusMemberRegex("[^A-Za-z0-9_]");

void tryIfaceInitialize(std::shared_ptr<sdbusplus::asio::dbus_interface>& iface)
{
    try
    {
        iface->initialize();
    }
    catch (std::exception& e)
    {
        std::cerr << "Unable to initialize dbus interface : " << e.what()
                  << "\n"
                  << "object Path : " << iface->get_object_path() << "\n"
                  << "interface name : " << iface->get_interface_name() << "\n";
    }
}

static std::shared_ptr<sdbusplus::asio::dbus_interface> createInterface(
    sdbusplus::asio::object_server& objServer, const std::string& path,
    const std::string& interface, const std::string& parent,
    bool checkNull = false)
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
                if (!configuration::setJsonFromPointer(jsonPointerString, val,
                                                       systemConfiguration))
                {
                    std::cerr << "error setting json field\n";
                    return -1;
                }
                if (!configuration::writeJsonFiles(systemConfiguration))
                {
                    std::cerr << "error setting json file\n";
                    return -1;
                }
                return 1;
            });
    }
}

template <typename PropertyType>
void addProperty(const std::string& name, const PropertyType& value,
                 sdbusplus::asio::dbus_interface* iface,
                 nlohmann::json& systemConfiguration,
                 const std::string& jsonPointerString,
                 sdbusplus::asio::PropertyPermission permission)
{
    if (permission == sdbusplus::asio::PropertyPermission::readOnly)
    {
        iface->register_property(name, value);
        return;
    }
    iface->register_property(
        name, value,
        [&systemConfiguration,
         jsonPointerString{std::string(jsonPointerString)}](
            const PropertyType& newVal, PropertyType& val) {
            val = newVal;
            if (!configuration::setJsonFromPointer(jsonPointerString, val,
                                                   systemConfiguration))
            {
                std::cerr << "error setting json field\n";
                return -1;
            }
            if (!configuration::writeJsonFiles(systemConfiguration))
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
            boost::asio::post(io, [&objServer, dbusInterface]() mutable {
                objServer.remove_interface(dbusInterface);
            });

            if (!configuration::writeJsonFiles(systemConfiguration))
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
    for (const auto& [key, value] : dict.items())
    {
        auto type = value.type();
        bool array = false;
        if (value.type() == nlohmann::json::value_t::array)
        {
            array = true;
            if (value.empty())
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
    tryIfaceInitialize(iface);
}

sdbusplus::asio::PropertyPermission getPermission(const std::string& interface)
{
    return std::find(settableInterfaces.begin(), settableInterfaces.end(),
                     interface) != settableInterfaces.end()
               ? sdbusplus::asio::PropertyPermission::readWrite
               : sdbusplus::asio::PropertyPermission::readOnly;
}

void createAddObjectMethod(
    const std::string& jsonPointerPath, const std::string& path,
    nlohmann::json& systemConfiguration,
    sdbusplus::asio::object_server& objServer, const std::string& board)
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
                std::visit(
                    [&newJson](auto&& val) {
                        newJson = std::forward<decltype(val)>(val);
                    },
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

            std::ifstream schemaFile(
                std::string(configuration::schemaDirectory) + "/" +
                boost::to_lower_copy(*type) + ".json");
            // todo(james) we might want to also make a list of 'can add'
            // interfaces but for now I think the assumption if there is a
            // schema avaliable that it is allowed to update is fine
            if (!schemaFile.good())
            {
                throw std::invalid_argument(
                    "No schema avaliable, cannot validate.");
            }
            nlohmann::json schema =
                nlohmann::json::parse(schemaFile, nullptr, false, true);
            if (schema.is_discarded())
            {
                std::cerr << "Schema not legal" << *type << ".json\n";
                throw DBusInternalError();
            }
            if (!configuration::validateJson(schema, newData))
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
            if (!configuration::writeJsonFiles(systemConfiguration))
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
    tryIfaceInitialize(iface);
}

void postToDbus(const nlohmann::json& newConfiguration,
                nlohmann::json& systemConfiguration,
                sdbusplus::asio::object_server& objServer)

{
    std::map<std::string, std::string> newBoards; // path -> name

    // iterate through boards
    for (const auto& [boardId, boardConfig] : newConfiguration.items())
    {
        std::string boardName = boardConfig["Name"];
        std::string boardNameOrig = boardConfig["Name"];
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
            std::cerr << "Unable to find type for " << boardName
                      << " reverting to Chassis.\n";
            boardType = "Chassis";
        }
        std::string boardtypeLower = boost::algorithm::to_lower_copy(boardType);

        std::regex_replace(boardName.begin(), boardName.begin(),
                           boardName.end(), illegalDbusMemberRegex, "_");
        std::string boardPath = "/xyz/openbmc_project/inventory/system/";
        boardPath += boardtypeLower;
        boardPath += "/";
        boardPath += boardName;

        std::shared_ptr<sdbusplus::asio::dbus_interface> inventoryIface =
            createInterface(objServer, boardPath,
                            "xyz.openbmc_project.Inventory.Item", boardName);

        std::shared_ptr<sdbusplus::asio::dbus_interface> boardIface =
            createInterface(objServer, boardPath,
                            "xyz.openbmc_project.Inventory.Item." + boardType,
                            boardNameOrig);

        createAddObjectMethod(jsonPointerPath, boardPath, systemConfiguration,
                              objServer, boardNameOrig);

        populateInterfaceFromJson(systemConfiguration, jsonPointerPath,
                                  boardIface, boardValues, objServer);
        jsonPointerPath += "/";
        // iterate through board properties
        for (const auto& [propName, propValue] : boardValues.items())
        {
            if (propValue.type() == nlohmann::json::value_t::object)
            {
                std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
                    createInterface(objServer, boardPath, propName,
                                    boardNameOrig);

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
            std::string ifacePath = boardPath;
            ifacePath += "/";
            ifacePath += itemName;

            if (itemType == "BMC")
            {
                std::shared_ptr<sdbusplus::asio::dbus_interface> bmcIface =
                    createInterface(objServer, ifacePath,
                                    "xyz.openbmc_project.Inventory.Item.Bmc",
                                    boardNameOrig);
                populateInterfaceFromJson(systemConfiguration, jsonPointerPath,
                                          bmcIface, item, objServer,
                                          getPermission(itemType));
            }
            else if (itemType == "System")
            {
                std::shared_ptr<sdbusplus::asio::dbus_interface> systemIface =
                    createInterface(objServer, ifacePath,
                                    "xyz.openbmc_project.Inventory.Item.System",
                                    boardNameOrig);
                populateInterfaceFromJson(systemConfiguration, jsonPointerPath,
                                          systemIface, item, objServer,
                                          getPermission(itemType));
            }

            for (const auto& [name, config] : item.items())
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
                                                      ifaceName, boardNameOrig);

                    populateInterfaceFromJson(
                        systemConfiguration, jsonPointerPath, objectIface,
                        config, objServer, getPermission(name));
                }
                else if (config.type() == nlohmann::json::value_t::array)
                {
                    size_t index = 0;
                    if (config.empty())
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
                                objServer, ifacePath, ifaceName, boardNameOrig);

                        populateInterfaceFromJson(
                            systemConfiguration,
                            jsonPointerPath + "/" + std::to_string(index),
                            objectIface, arrayItem, objServer,
                            getPermission(name));
                        index++;
                    }
                }
            }

            std::shared_ptr<sdbusplus::asio::dbus_interface> itemIface =
                createInterface(objServer, ifacePath,
                                "xyz.openbmc_project.Configuration." + itemType,
                                boardNameOrig);

            populateInterfaceFromJson(systemConfiguration, jsonPointerPath,
                                      itemIface, item, objServer,
                                      getPermission(itemType));

            topology.addBoard(boardPath, boardType, boardNameOrig, item);
        }

        newBoards.emplace(boardPath, boardNameOrig);
    }

    for (const auto& [assocPath, assocPropValue] :
         topology.getAssocs(newBoards))
    {
        auto findBoard = newBoards.find(assocPath);
        if (findBoard == newBoards.end())
        {
            continue;
        }

        auto ifacePtr = createInterface(
            objServer, assocPath, "xyz.openbmc_project.Association.Definitions",
            findBoard->second);

        ifacePtr->register_property("Associations", assocPropValue);
        tryIfaceInitialize(ifacePtr);
    }
}

static bool deviceRequiresPowerOn(const nlohmann::json& entity)
{
    auto powerState = entity.find("PowerState");
    if (powerState == entity.end())
    {
        return false;
    }

    const auto* ptr = powerState->get_ptr<const std::string*>();
    if (ptr == nullptr)
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
    topology.remove(device["Name"].get<std::string>());
    logDeviceRemoved(device);
}

static void publishNewConfiguration(
    const size_t& instance, const size_t count,
    boost::asio::steady_timer& timer, nlohmann::json& systemConfiguration,
    // Gerrit discussion:
    // https://gerrit.openbmc-project.xyz/c/openbmc/entity-manager/+/52316/6
    //
    // Discord discussion:
    // https://discord.com/channels/775381525260664832/867820390406422538/958048437729910854
    //
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    const nlohmann::json newConfiguration,
    sdbusplus::asio::object_server& objServer)
{
    loadOverlays(newConfiguration);

    boost::asio::post(io, [systemConfiguration]() {
        if (!configuration::writeJsonFiles(systemConfiguration))
        {
            std::cerr << "Error writing json files\n";
        }
    });

    boost::asio::post(io, [&instance, count, &timer, newConfiguration,
                           &systemConfiguration, &objServer]() {
        postToDbus(newConfiguration, systemConfiguration, objServer);
        if (count == instance)
        {
            startRemovedTimer(timer, systemConfiguration);
        }
    });
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

    timer.expires_after(std::chrono::milliseconds(500));

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
        if (!configuration::loadConfigurations(configurations))
        {
            std::cerr << "Could not load configurations\n";
            inProgress = false;
            return;
        }

        auto perfScan = std::make_shared<scan::PerformScan>(
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

                configuration::deriveNewConfiguration(oldConfiguration,
                                                      newConfiguration);

                for (const auto& [_, device] : newConfiguration.items())
                {
                    logDeviceAdded(device);
                }

                inProgress = false;

                boost::asio::post(
                    io, std::bind_front(
                            publishNewConfiguration, std::ref(instance), count,
                            std::ref(timer), std::ref(systemConfiguration),
                            newConfiguration, std::ref(objServer)));
            });
        perfScan->run();
    });
}

// Check if InterfacesAdded payload contains an iface that needs probing.
static bool iaContainsProbeInterface(
    sdbusplus::message_t& msg, const std::set<std::string>& probeInterfaces)
{
    sdbusplus::message::object_path path;
    DBusObject interfaces;
    std::set<std::string> interfaceSet;
    std::set<std::string> intersect;

    msg.read(path, interfaces);

    std::for_each(interfaces.begin(), interfaces.end(),
                  [&interfaceSet](const auto& iface) {
                      interfaceSet.insert(iface.first);
                  });

    std::set_intersection(interfaceSet.begin(), interfaceSet.end(),
                          probeInterfaces.begin(), probeInterfaces.end(),
                          std::inserter(intersect, intersect.end()));
    return !intersect.empty();
}

// Check if InterfacesRemoved payload contains an iface that needs probing.
static bool irContainsProbeInterface(
    sdbusplus::message_t& msg, const std::set<std::string>& probeInterfaces)
{
    sdbusplus::message::object_path path;
    std::set<std::string> interfaces;
    std::set<std::string> intersect;

    msg.read(path, interfaces);

    std::set_intersection(interfaces.begin(), interfaces.end(),
                          probeInterfaces.begin(), probeInterfaces.end(),
                          std::inserter(intersect, intersect.end()));
    return !intersect.empty();
}

int main()
{
    // setup connection to dbus
    systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    systemBus->request_name("xyz.openbmc_project.EntityManager");

    // The EntityManager object itself doesn't expose any properties.
    // No need to set up ObjectManager for the |EntityManager| object.
    sdbusplus::asio::object_server objServer(systemBus, /*skipManager=*/true);

    // All other objects that EntityManager currently support are under the
    // inventory subtree.
    // See the discussion at
    // https://discord.com/channels/775381525260664832/1018929092009144380
    objServer.add_manager("/xyz/openbmc_project/inventory");

    std::shared_ptr<sdbusplus::asio::dbus_interface> entityIface =
        objServer.add_interface("/xyz/openbmc_project/EntityManager",
                                "xyz.openbmc_project.EntityManager");

    // to keep reference to the match / filter objects so they don't get
    // destroyed

    nlohmann::json systemConfiguration = nlohmann::json::object();

    std::set<std::string> probeInterfaces = configuration::getProbeInterfaces();

    // We need a poke from DBus for static providers that create all their
    // objects prior to claiming a well-known name, and thus don't emit any
    // org.freedesktop.DBus.Properties signals.  Similarly if a process exits
    // for any reason, expected or otherwise, we'll need a poke to remove
    // entities from DBus.
    sdbusplus::bus::match_t nameOwnerChangedMatch(
        static_cast<sdbusplus::bus_t&>(*systemBus),
        sdbusplus::bus::match::rules::nameOwnerChanged(),
        [&](sdbusplus::message_t& m) {
            auto [name, oldOwner,
                  newOwner] = m.unpack<std::string, std::string, std::string>();

            if (name.starts_with(':'))
            {
                // We should do nothing with unique-name connections.
                return;
            }

            propertiesChangedCallback(systemConfiguration, objServer);
        });
    // We also need a poke from DBus when new interfaces are created or
    // destroyed.
    sdbusplus::bus::match_t interfacesAddedMatch(
        static_cast<sdbusplus::bus_t&>(*systemBus),
        sdbusplus::bus::match::rules::interfacesAdded(),
        [&](sdbusplus::message_t& msg) {
            if (iaContainsProbeInterface(msg, probeInterfaces))
            {
                propertiesChangedCallback(systemConfiguration, objServer);
            }
        });
    sdbusplus::bus::match_t interfacesRemovedMatch(
        static_cast<sdbusplus::bus_t&>(*systemBus),
        sdbusplus::bus::match::rules::interfacesRemoved(),
        [&](sdbusplus::message_t& msg) {
            if (irContainsProbeInterface(msg, probeInterfaces))
            {
                propertiesChangedCallback(systemConfiguration, objServer);
            }
        });

    boost::asio::post(io, [&]() {
        propertiesChangedCallback(systemConfiguration, objServer);
    });

    entityIface->register_method("ReScan", [&]() {
        propertiesChangedCallback(systemConfiguration, objServer);
    });
    tryIfaceInitialize(entityIface);

    if (fwVersionIsSame())
    {
        if (std::filesystem::is_regular_file(
                configuration::currentConfiguration))
        {
            // this file could just be deleted, but it's nice for debug
            std::filesystem::create_directory(tempConfigDir);
            std::filesystem::remove(lastConfiguration);
            std::filesystem::copy(configuration::currentConfiguration,
                                  lastConfiguration);
            std::filesystem::remove(configuration::currentConfiguration);

            std::ifstream jsonStream(lastConfiguration);
            if (jsonStream.good())
            {
                auto data = nlohmann::json::parse(jsonStream, nullptr, false);
                if (data.is_discarded())
                {
                    std::cerr
                        << "syntax error in " << lastConfiguration << "\n";
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
        std::filesystem::remove(configuration::currentConfiguration);
    }

    // some boards only show up after power is on, we want to not say they are
    // removed until the same state happens
    setupPowerMatch(systemBus);

    io.run();

    return 0;
}
