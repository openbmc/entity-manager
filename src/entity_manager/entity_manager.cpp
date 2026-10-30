// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "entity_manager.hpp"

#include "../dbus_util.hpp"
#include "../utils.hpp"
#include "../variant_visitors.hpp"
#include "board_presence.hpp"
#include "configuration.hpp"
#include "dbus_interface.hpp"
#include "log_device_inventory.hpp"
#include "overlay.hpp"
#include "perform_scan.hpp"
#include "topology.hpp"
#include "utils.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/range/iterator_range.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Association/Definitions/common.hpp>
#include <xyz/openbmc_project/Inventory/Item/Bmc/common.hpp>
#include <xyz/openbmc_project/Inventory/Item/System/common.hpp>
#include <xyz/openbmc_project/Inventory/Item/common.hpp>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <flat_map>
#include <flat_set>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <regex>
#include <string>
constexpr const char* tempConfigDir = "/tmp/configuration/";
constexpr const char* lastConfiguration = "/tmp/configuration/last.json";

static constexpr std::array<const char*, 6> settableInterfaces = {
    "FanProfile", "Pid", "Pid.Zone", "Stepwise", "Thresholds", "Polling"};

sdbusplus::asio::PropertyPermission getPermission(const std::string& interface)
{
    return std::find(settableInterfaces.begin(), settableInterfaces.end(),
                     interface) != settableInterfaces.end()
               ? sdbusplus::asio::PropertyPermission::readWrite
               : sdbusplus::asio::PropertyPermission::readOnly;
}

EntityManager::EntityManager(
    std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    boost::asio::io_context& io,
    const std::vector<std::filesystem::path>& configurationDirectories,
    const std::filesystem::path& schemaDirectory,
    std::filesystem::path configCacheDir) :
    systemBus(systemBus),
    objServer(sdbusplus::asio::object_server(systemBus, /*skipManager=*/true)),
    configuration(configurationDirectories, schemaDirectory),
    lastJson(nlohmann::json::object()),
    systemConfiguration(nlohmann::json::object()), io(io),
    configCache(std::move(configCacheDir)),
    dbus_interface(io, objServer, schemaDirectory, configCache),
    powerStatus(*systemBus), propertiesChangedTimer(io)
{
    // All other objects that EntityManager currently support are under the
    // inventory subtree.
    // See the discussion at
    // https://discord.com/channels/775381525260664832/1018929092009144380
    objServer.add_manager("/xyz/openbmc_project/inventory");

    auto entityIface = objServer.add_interface(emDbusPath, emDbusName);
    entityIface->register_method("ReScan", [this]() {
        propertiesChangedCallback();
    });
    dbus_interface::tryIfaceInitialize(entityIface);

    initFilters(configuration.probeInterfaces);
}

void EntityManager::postToDbus(const nlohmann::json& newConfiguration)
{
    std::map<sdbusplus::object_path, std::string> newObjects; // path -> name

    // iterate through configurations
    for (const auto& [configId, configObject] : newConfiguration.items())
    {
        const nlohmann::json::object_t* configObjectPtr =
            configObject.get_ptr<const nlohmann::json::object_t*>();
        if (configObjectPtr == nullptr)
        {
            lg2::error("configObject for {CONFIG} was not an object", "CONFIG",
                       configId);
            continue;
        }
        postBoardToDBus(configId, *configObjectPtr, newObjects);
    }

    for (const auto& [assocPath, assocPropValue] :
         topology.getAssocs(std::views::keys(newObjects)))
    {
        auto findBoard = newObjects.find(assocPath);
        if (findBoard == newObjects.end())
        {
            continue;
        }

        auto ifacePtr = dbus_interface.createInterface(
            assocPath,
            sdbusplus::common::xyz::openbmc_project::association::Definitions::
                interface,
            findBoard->second);

        ifacePtr->register_property("Associations", assocPropValue);
        dbus_interface::tryIfaceInitialize(ifacePtr);
    }
}

void EntityManager::postProbeConfig(
    const sdbusplus::object_path& objectPath, const std::string& configName,
    const std::string& configType, const nlohmann::json& probe)
{
    std::vector<std::string> probeStatements;
    const std::string* single = probe.get_ptr<const std::string*>();
    if (single != nullptr)
    {
        // one statement, but the property is always an array
        probeStatements.emplace_back(*single);
    }
    else
    {
        probeStatements = probe.get<std::vector<std::string>>();
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        dbus_interface.createInterface(
            objectPath, "xyz.openbmc_project.Configuration.Probe", configName);

    iface->register_property("Name", configName);
    iface->register_property("Type", configType);
    iface->register_property("Probe", probeStatements);

    dbus_interface::tryIfaceInitialize(iface);
}

void EntityManager::postBoardToDBus(
    const std::string& configId, const nlohmann::json::object_t& configObject,
    std::map<sdbusplus::object_path, std::string>& newObjects)
{
    auto configNameIt = configObject.find("Name");
    if (configNameIt == configObject.end())
    {
        lg2::error("Unable to find name for {CONFIG}", "CONFIG", configId);
        return;
    }
    const std::string* configNamePtr =
        configNameIt->second.get_ptr<const std::string*>();
    if (configNamePtr == nullptr)
    {
        lg2::error("Name for {CONFIG} was not a string", "CONFIG", configId);
        return;
    }
    std::string configName = *configNamePtr;
    std::string configNameOrig = *configNamePtr;
    // loop through newConfiguration, but use values from system
    // configuration to be able to modify via dbus later
    auto configValues = systemConfiguration[configId];
    std::optional<std::string> resolvedType =
        em_utils::resolveConfigType(configValues);
    if (!resolvedType)
    {
        lg2::error(
            "Type for {CONFIG} was missing, not a string, or not a valid "
            "D-Bus path segment, dropping",
            "CONFIG", configName);
        return;
    }
    const std::string& configType = *resolvedType;

    lg2::debug("post {TYPE} '{NAME}' to DBus", "TYPE", configType, "NAME",
               configName);

    const sdbusplus::object_path objectPath =
        em_utils::buildInventorySystemPath(configName, configType);

    std::shared_ptr<sdbusplus::asio::dbus_interface> inventoryIface =
        dbus_interface.createInterface(
            objectPath,
            sdbusplus::common::xyz::openbmc_project::inventory::Item::interface,
            configNameOrig);

    dbus_interface.createAddObjectMethod(configId, objectPath,
                                         systemConfiguration, configNameOrig);

    // A configuration type only gets a top-level interface if it is listed
    // here. Adding a type to the schema is therefore not enough to make
    // entity-manager claim an interface for it, which keeps us from exporting
    // interfaces that aren't defined in phosphor-dbus-interfaces.
    static const std::flat_map<std::string, std::string> typeToInterface = {
        {"Board", "xyz.openbmc_project.Inventory.Item.Board"},
        {"Cable", "xyz.openbmc_project.Inventory.Item.Cable"},
        {"Chassis", "xyz.openbmc_project.Inventory.Item.Chassis"},
        {"Cpu", "xyz.openbmc_project.Inventory.Item.Cpu"},
        // NVMe has no phosphor-dbus-interfaces definition, but is exported
        // today and consumers rely on it.
        {"NVMe", "xyz.openbmc_project.Inventory.Item.NVMe"},
        {"PowerSupply", "xyz.openbmc_project.Inventory.Item.PowerSupply"},
        {"Valve", "xyz.openbmc_project.Inventory.Item.Valve"},
        // A platform is not an inventory item, so it is named under the
        // Configuration namespace instead.
        {"Platform", "xyz.openbmc_project.Configuration.Platform"},
    };

    std::shared_ptr<sdbusplus::asio::dbus_interface> typeIface;
    auto findInterface = typeToInterface.find(configType);
    const std::string invItemIntf = findInterface != typeToInterface.end()
                                        ? findInterface->second
                                        : std::string{};

    auto findProbe = configValues.find("Probe");
    if (findProbe != configValues.end())
    {
        postProbeConfig(objectPath, configNameOrig, configType, *findProbe);
    }

    // iterate through configuration properties
    for (const auto& [propName, propValue] : configValues.items())
    {
        if (const auto* propValueObj =
                propValue.get_ptr<const nlohmann::json::object_t*>())
        {
            std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
                dbus_interface.createInterface(objectPath, propName,
                                               configNameOrig);
            if (propName == invItemIntf)
            {
                typeIface = iface;
            }

            dbus_interface.populateInterfaceFromJson(
                systemConfiguration, ConfigPointer(configId, propName), iface,
                *propValueObj);
        }
    }

    if (typeIface == nullptr && !invItemIntf.empty())
    {
        // Name, Type and Probe say how the configuration was matched and
        // are published on Configuration.Probe.
        typeIface = dbus_interface.createInterface(objectPath, invItemIntf,
                                                   configNameOrig);
        dbus_interface::tryIfaceInitialize(typeIface);
    }

    nlohmann::json::iterator exposes = configValues.find("Exposes");
    if (exposes == configValues.end())
    {
        return;
    }

    std::optional<board_presence::Names> foundBoards;
    if (configType == "Platform")
    {
        foundBoards = board_presence::foundNames(systemConfiguration);
    }

    for (size_t exposesIndex = 0; exposesIndex < exposes->size();
         ++exposesIndex)
    {
        const nlohmann::json::object_t* itemObj =
            (*exposes)[exposesIndex].get_ptr<const nlohmann::json::object_t*>();

        if (itemObj == nullptr)
        {
            // null slot
            lg2::debug("Exposes record was not an object");
            continue;
        }
        std::optional<sdbusplus::object_path> itemPath =
            postExposesRecordsToDBus(*itemObj, exposesIndex, configNameOrig,
                                     configId, objectPath, configType,
                                     foundBoards ? &*foundBoards : nullptr);
        if (itemPath)
        {
            dbus_interface.addConfigObject(*itemPath, configNameOrig);
        }
    }

    newObjects.emplace(objectPath, configNameOrig);
}

// if status is not found it is assumed to be status = 'okay'
static bool recordDisabled(const nlohmann::json::object_t& item)
{
    auto findStatus = item.find("Status");
    return findStatus != item.end() && findStatus->second == "disabled";
}

static sdbusplus::object_path recordPath(
    const sdbusplus::object_path& objectPath, const std::string& recordName)
{
    return objectPath / dbus_util::sanitizeForDBusPathSegment(recordName);
}

static std::string arrayObjectInterface(
    const std::string& itemType, const std::string& property, size_t index)
{
    std::string ifaceName = "xyz.openbmc_project.Configuration.";
    ifaceName.append(itemType).append(".").append(property);
    ifaceName.append(std::to_string(index));
    return ifaceName;
}

std::optional<sdbusplus::object_path> EntityManager::postExposesRecordsToDBus(
    const nlohmann::json::object_t& item, size_t exposesIndex,
    const std::string& configNameOrig, const std::string& boardId,
    const sdbusplus::object_path& objectPath, const std::string& configType,
    const board_presence::Names* foundBoards)
{
    const ConfigPointer configPointerPath(boardId, exposesIndex);

    auto findName = item.find("Name");
    if (findName == item.end())
    {
        lg2::error("cannot find name in field {ITEM}", "ITEM",
                   nlohmann::json(item));
        return std::nullopt;
    }
    if (recordDisabled(item))
    {
        return std::nullopt;
    }
    if (foundBoards != nullptr &&
        !board_presence::boardFound(item, *foundBoards))
    {
        lg2::info("{CONFIG} record {NAME} waits for board {BOARD}", "CONFIG",
                  configNameOrig, "NAME", findName->second, "BOARD",
                  item.at("Board").get<std::string>());
        return std::nullopt;
    }
    auto findType = item.find("Type");
    std::string itemType = "unknown";
    if (findType != item.end())
    {
        itemType = findType->second.get<std::string>();
    }

    if (!dbus_util::validateDBusInterfaceSegments(itemType))
    {
        lg2::error(
            "item Type '{TYPE}' is not a valid D-Bus interface segment(s)",
            "TYPE", itemType);
        return std::nullopt;
    }

    sdbusplus::object_path ifacePath =
        recordPath(objectPath, findName->second.get<std::string>());

    if (itemType == "BMC")
    {
        std::shared_ptr<sdbusplus::asio::dbus_interface> bmcIface =
            dbus_interface.createInterface(
                ifacePath,
                sdbusplus::common::xyz::openbmc_project::inventory::item::Bmc::
                    interface,
                configNameOrig);
        dbus_interface.populateInterfaceFromJson(
            systemConfiguration, configPointerPath, bmcIface, item,
            getPermission(itemType));
    }
    else if (itemType == "System")
    {
        std::shared_ptr<sdbusplus::asio::dbus_interface> systemIface =
            dbus_interface.createInterface(
                ifacePath,
                sdbusplus::common::xyz::openbmc_project::inventory::item::
                    System::interface,
                configNameOrig);
        dbus_interface.populateInterfaceFromJson(
            systemConfiguration, configPointerPath, systemIface, item,
            getPermission(itemType));
    }

    for (const auto& [name, config] : item)
    {
        if (!postConfigurationRecord(
                name, config, configNameOrig, itemType,
                configPointerPath.withExposesIndexAndName(exposesIndex, name),
                ifacePath, foundBoards))
        {
            break;
        }
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> itemIface =
        dbus_interface.createInterface(
            ifacePath, "xyz.openbmc_project.Configuration." + itemType,
            configNameOrig);

    dbus_interface.populateInterfaceFromJson(
        systemConfiguration, configPointerPath, itemIface, item,
        getPermission(itemType));

    topology.addBoard(objectPath, configType, item);

    if (!itemIface->is_initialized())
    {
        return std::nullopt;
    }
    return ifacePath;
}

void EntityManager::postArrayObject(
    const std::string& name, size_t index,
    const nlohmann::json::object_t& object, const std::string& configNameOrig,
    const std::string& itemType, const ConfigPointer& configPtr,
    const sdbusplus::object_path& ifacePath)
{
    std::shared_ptr<sdbusplus::asio::dbus_interface> objectIface =
        dbus_interface.createInterface(
            ifacePath, arrayObjectInterface(itemType, name, index),
            configNameOrig);

    dbus_interface.populateInterfaceFromJson(
        systemConfiguration, configPtr.withArrayIndex(index), objectIface,
        object, getPermission(name));
}

bool EntityManager::postConfigurationRecord(
    const std::string& name, const nlohmann::json& config,
    const std::string& configNameOrig, const std::string& itemType,
    const ConfigPointer& configPtr, const sdbusplus::object_path& ifacePath,
    const board_presence::Names* foundBoards)
{
    if (const auto* configObj =
            config.get_ptr<const nlohmann::json::object_t*>())
    {
        std::string ifaceName = "xyz.openbmc_project.Configuration.";
        ifaceName.append(itemType).append(".").append(name);

        std::shared_ptr<sdbusplus::asio::dbus_interface> objectIface =
            dbus_interface.createInterface(ifacePath, ifaceName,
                                           configNameOrig);

        dbus_interface.populateInterfaceFromJson(
            systemConfiguration, configPtr, objectIface, *configObj,
            getPermission(name));
    }
    else if (config.type() == nlohmann::json::value_t::array)
    {
        size_t index = 0;
        if (config.empty())
        {
            return true;
        }
        bool isLegal = true;
        auto type = config[0].type();
        if (type != nlohmann::json::value_t::object)
        {
            return true;
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
            lg2::error("dbus format error {JSON}", "JSON", config);
            return false;
        }

        for (const auto& arrayItem : config)
        {
            const nlohmann::json::object_t* arrayItemObj =
                arrayItem.get_ptr<const nlohmann::json::object_t*>();

            // Keep the index of an object that waits for its board, since
            // the position in the array can carry meaning of its own.
            if (foundBoards != nullptr &&
                !board_presence::boardFound(*arrayItemObj, *foundBoards))
            {
                lg2::info("{CONFIG} {NAME}{INDEX} waits for board {BOARD}",
                          "CONFIG", configNameOrig, "NAME", name, "INDEX",
                          index, "BOARD",
                          arrayItemObj->at("Board").get<std::string>());
                index++;
                continue;
            }

            postArrayObject(name, index, *arrayItemObj, configNameOrig,
                            itemType, configPtr, ifacePath);
            index++;
        }
    }

    return true;
}

static void pruneDevice(const nlohmann::json& systemConfiguration,
                        std::unordered_set<std::string>& cachedBaseline,
                        const std::string& name, const nlohmann::json& device)
{
    if (systemConfiguration.contains(name) || !cachedBaseline.contains(name))
    {
        return;
    }

    logDeviceRemoved(name, device, cachedBaseline);
}

void EntityManager::startRemovedTimer(boost::asio::steady_timer& timer)
{
    if (systemConfiguration.empty() || lastJson.empty())
    {
        return; // not ready yet
    }
    if (scannedPowerOn)
    {
        return;
    }

    if (!powerStatus.isPowerOn() && scannedPowerOff)
    {
        return;
    }

    timer.expires_after(std::chrono::seconds(10));
    timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            return;
        }

        bool powerOff = !powerStatus.isPowerOn();
        for (const auto& [name, device] : lastJson.items())
        {
            pruneDevice(systemConfiguration, cachedBaseline, name, device);
        }

        scannedPowerOff = true;
        if (!powerOff)
        {
            scannedPowerOn = true;
        }
    });
}

void EntityManager::pruneConfiguration(const std::string& name,
                                       const nlohmann::json& device)
{
    lg2::debug("pruning configuration");

    dbus_interface.removeDeviceInterfaces(device);
    systemConfiguration.erase(name);
    std::optional<std::string> configType = em_utils::resolveConfigType(device);
    if (configType)
    {
        std::string boardName = device["Name"].get<std::string>();
        topology.remove(
            em_utils::buildInventorySystemPath(boardName, *configType));
    }
    logDeviceRemoved(name, device, cachedBaseline);
}

void EntityManager::removeInterfaces(
    const nlohmann::json& config,
    const std::function<bool(sdbusplus::asio::dbus_interface&)>& match)
{
    auto& ifaces = dbus_interface.getDeviceInterfaces(config);
    std::erase_if(ifaces, [&](const auto& weakIface) {
        auto iface = weakIface.lock();
        if (!iface)
        {
            return true;
        }
        if (!match(*iface))
        {
            return false;
        }
        objServer.remove_interface(iface);
        return true;
    });
}

void EntityManager::refreshPlatformRecords(
    const board_presence::Names& before, const board_presence::Names& after,
    const std::flat_set<std::string>& newConfigIds)
{
    if (before == after)
    {
        return;
    }

    for (const auto& [configId, config] : systemConfiguration.items())
    {
        if (newConfigIds.contains(configId) ||
            em_utils::resolveConfigType(config) != "Platform")
        {
            continue;
        }
        auto exposes = config.find("Exposes");
        auto configName = config.find("Name");
        if (exposes == config.end() || !exposes->is_array() ||
            configName == config.end() || !configName->is_string())
        {
            continue;
        }
        const std::string& name = configName->get_ref<const std::string&>();
        std::string pathName = name;
        const sdbusplus::object_path objectPath =
            em_utils::buildInventorySystemPath(pathName, "Platform");

        for (size_t exposesIndex = 0; exposesIndex < exposes->size();
             ++exposesIndex)
        {
            const auto* item = (*exposes)[exposesIndex]
                                   .get_ptr<const nlohmann::json::object_t*>();
            if (item == nullptr || recordDisabled(*item))
            {
                continue;
            }
            auto itemName = item->find("Name");
            auto itemType = item->find("Type");
            if (itemName == item->end() || !itemName->second.is_string() ||
                itemType == item->end() || !itemType->second.is_string() ||
                !dbus_util::validateDBusInterfaceSegments(
                    itemType->second.get_ref<const std::string&>()))
            {
                continue;
            }

            const board_presence::RecordChange change =
                board_presence::recordChange(*item, before, after);
            if (change.action == board_presence::RecordChange::Action::none)
            {
                continue;
            }

            // Only what changed is touched, so a consumer of the rest of the
            // record does not see it go away and come back.
            const sdbusplus::object_path ifacePath =
                recordPath(objectPath, itemName->second.get<std::string>());
            const std::string& type =
                itemType->second.get_ref<const std::string&>();
            lg2::info("{CONFIG} record {NAME} follows a board change", "CONFIG",
                      name, "NAME", itemName->second);

            switch (change.action)
            {
                case board_presence::RecordChange::Action::publish:
                {
                    std::optional<sdbusplus::object_path> itemPath =
                        postExposesRecordsToDBus(*item, exposesIndex, name,
                                                 configId, objectPath,
                                                 "Platform", &after);
                    if (itemPath)
                    {
                        dbus_interface.addConfigObject(*itemPath, name);
                    }
                    break;
                }
                case board_presence::RecordChange::Action::withdraw:
                {
                    // Another record of the same Name may share the object,
                    // so only this record's interfaces go.
                    const std::string recordIface =
                        "xyz.openbmc_project.Configuration." + type;
                    removeInterfaces(config, [&](auto& iface) {
                        const std::string& ifaceName =
                            iface.get_interface_name();
                        return iface.get_object_path() == ifacePath &&
                               (ifaceName == recordIface ||
                                ifaceName.starts_with(recordIface + "."));
                    });
                    dbus_interface.removeConfigObject(ifacePath);
                    break;
                }
                case board_presence::RecordChange::Action::update:
                    for (const auto& object : change.removed)
                    {
                        const std::string iface = arrayObjectInterface(
                            type, object.property, object.index);
                        removeInterfaces(config, [&](auto& candidate) {
                            return candidate.get_object_path() == ifacePath &&
                                   candidate.get_interface_name() == iface;
                        });
                    }
                    for (const auto& object : change.added)
                    {
                        postArrayObject(
                            object.property, object.index,
                            item->at(object.property)
                                .at(object.index)
                                .get_ref<const nlohmann::json::object_t&>(),
                            name, type,
                            ConfigPointer(configId, exposesIndex)
                                .withExposesIndexAndName(exposesIndex,
                                                         object.property),
                            ifacePath);
                    }
                    break;
                case board_presence::RecordChange::Action::none:
                    break;
            }
        }
    }
}

void EntityManager::publishNewConfiguration(
    const size_t& instance, const size_t count,
    boost::asio::steady_timer& timer, // Gerrit discussion:
    // https://gerrit.openbmc-project.xyz/c/openbmc/entity-manager/+/52316/6
    //
    // Discord discussion:
    // https://discord.com/channels/775381525260664832/867820390406422538/958048437729910854
    //
    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    const nlohmann::json newConfiguration)
{
    loadOverlays(newConfiguration, io);

    boost::asio::post(io, [this]() {
        if (!configCache.writeJsonFiles(systemConfiguration))
        {
            lg2::error("Error writing json files");
        }
    });

    boost::asio::post(io, [this, &instance, count, &timer, newConfiguration]() {
        postToDbus(newConfiguration);
        if (count == instance)
        {
            startRemovedTimer(timer);
        }
    });
}

void EntityManager::propertiesChangedCallbackDebounced(
    const size_t count, const boost::system::error_code& ec)
{
    lg2::debug("properties changed callback timer expired");
    if (ec == boost::asio::error::operation_aborted)
    {
        // we were cancelled
        return;
    }
    if (ec)
    {
        lg2::error("async wait error {ERR}", "ERR", ec.message());
        return;
    }

    if (propertiesChangedInProgress)
    {
        propertiesChangedCallback();
        return;
    }
    propertiesChangedInProgress = true;

    lg2::debug("properties changed callback in progress");

    nlohmann::json oldConfiguration = systemConfiguration;
    auto missingConfigurations = std::make_shared<nlohmann::json>();
    *missingConfigurations = systemConfiguration;

    auto perfScan = std::make_shared<scan::PerformScan>(
        *this, *missingConfigurations, configuration.configurations, io,
        [this, count, oldConfiguration, missingConfigurations]() {
            // this is something that since ac has been applied to the
            // bmc we saw, and we no longer see it
            for (const auto& [name, device] : missingConfigurations->items())
            {
                pruneConfiguration(name, device);
            }
            nlohmann::json newConfiguration = systemConfiguration;

            deriveNewConfiguration(oldConfiguration, newConfiguration);

            for (const auto& [key, device] : newConfiguration.items())
            {
                if (!cachedBaseline.contains(key))
                {
                    logDeviceAdded(device);
                }
            }

            board_presence::Names boardsBefore =
                board_presence::foundNames(oldConfiguration);
            board_presence::Names boardsAfter =
                board_presence::foundNames(systemConfiguration);
            std::flat_set<std::string> newConfigIds;
            for (const auto& [key, _] : newConfiguration.items())
            {
                newConfigIds.emplace(key);
            }

            propertiesChangedInProgress = false;

            boost::asio::post(
                io, [this, newConfiguration, count,
                     boardsBefore{std::move(boardsBefore)},
                     boardsAfter{std::move(boardsAfter)},
                     newConfigIds{std::move(newConfigIds)}]() mutable {
                    publishNewConfiguration(
                        std::ref(propertiesChangedInstance), count,
                        std::ref(propertiesChangedTimer), newConfiguration);
                    // After the boards themselves are published.
                    boost::asio::post(
                        io, [this, boardsBefore{std::move(boardsBefore)},
                             boardsAfter{std::move(boardsAfter)},
                             newConfigIds{std::move(newConfigIds)}] {
                            refreshPlatformRecords(boardsBefore, boardsAfter,
                                                   newConfigIds);
                        });
                });
        });
    perfScan->run();
}

// main properties changed entry
void EntityManager::propertiesChangedCallback()
{
    lg2::debug("properties changed callback");
    propertiesChangedInstance++;
    size_t count = propertiesChangedInstance;

    propertiesChangedTimer.expires_after(std::chrono::milliseconds(500));

    // setup an async wait as we normally get flooded with new requests
    propertiesChangedTimer.async_wait(std::bind_front(
        &EntityManager::propertiesChangedCallbackDebounced, this, count));
}

// Check if InterfacesAdded payload contains an iface that needs probing.
static bool iaContainsProbeInterface(
    sdbusplus::message_t& msg,
    const std::unordered_set<std::string>& probeInterfaces)
{
    sdbusplus::object_path path;
    DBusObject interfaces;
    msg.read(path, interfaces);
    return std::ranges::any_of(interfaces | std::views::keys,
                               [&probeInterfaces](const auto& ifaceName) {
                                   return probeInterfaces.contains(ifaceName);
                               });
}

// Check if InterfacesRemoved payload contains an iface that needs probing.
static bool irContainsProbeInterface(
    const std::vector<std::string>& interfaces,
    const std::unordered_set<std::string>& probeInterfaces)
{
    return std::ranges::any_of(interfaces,
                               [&probeInterfaces](const auto& ifaceName) {
                                   return probeInterfaces.contains(ifaceName);
                               });
}

void EntityManager::handleCurrentConfigurationJson()
{
    if (EM_CACHE_CONFIGURATION &&
        em_utils::fwVersionIsSame(configCache.configurationOutDir,
                                  configCache.versionHashFile))
    {
        if (std::filesystem::is_regular_file(configCache.currentConfiguration))
        {
            // this file could just be deleted, but it's nice for debug
            std::filesystem::create_directory(tempConfigDir);
            std::filesystem::remove(lastConfiguration);
            std::filesystem::copy(configCache.currentConfiguration,
                                  lastConfiguration);
            std::filesystem::remove(configCache.currentConfiguration);

            std::ifstream jsonStream(lastConfiguration);
            if (jsonStream.good())
            {
                auto data = nlohmann::json::parse(jsonStream, nullptr, false);
                if (data.is_discarded())
                {
                    lg2::error("syntax error in {PATH}", "PATH",
                               lastConfiguration);
                }
                else
                {
                    lastJson = std::move(data);
                    for (const auto& [key, _] : lastJson.items())
                    {
                        cachedBaseline.insert(key);
                    }
                }
            }
            else
            {
                lg2::error("unable to open {PATH}", "PATH", lastConfiguration);
            }
        }
    }
    else
    {
        // not an error, just logging at this level to make it in the journal
        std::error_code ec;
        lg2::error("Clearing previous configuration");
        std::filesystem::remove(configCache.currentConfiguration, ec);
    }
}

void EntityManager::registerCallback(const sdbusplus::object_path& path)
{
    if (dbusMatches.contains(path))
    {
        return;
    }

    lg2::debug("creating PropertiesChanged match on {PATH}", "PATH", path);

    std::function<void(sdbusplus::message_t & message)> eventHandler =
        [&](sdbusplus::message_t&) { propertiesChangedCallback(); };

    sdbusplus::match match(
        static_cast<sdbusplus::bus_t&>(*systemBus),
        "type='signal',member='PropertiesChanged',path='" + path.string() + "'",
        eventHandler);
    dbusMatches.emplace(path, std::move(match));
}

// We need a poke from DBus for static providers that create all their
// objects prior to claiming a well-known name, and thus don't emit any
// org.freedesktop.DBus.Properties signals.  Similarly if a process exits
// for any reason, expected or otherwise, we'll need a poke to remove
// entities from DBus.
void EntityManager::initFilters(
    const std::unordered_set<std::string>& probeInterfaces)
{
    nameOwnerChangedMatch = std::make_unique<sdbusplus::match>(
        static_cast<sdbusplus::bus_t&>(*systemBus),
        sdbusplus::match_rules::nameOwnerChanged(),
        [this](sdbusplus::message_t& m) {
            auto [name, oldOwner,
                  newOwner] = m.unpack<std::string, std::string, std::string>();

            if (name.starts_with(':'))
            {
                // We should do nothing with unique-name connections.
                return;
            }

            propertiesChangedCallback();
        });

    // We also need a poke from DBus when new interfaces are created or
    // destroyed.
    interfacesAddedMatch = std::make_unique<sdbusplus::match>(
        static_cast<sdbusplus::bus_t&>(*systemBus),
        sdbusplus::match_rules::interfacesAdded(),
        [this, probeInterfaces](sdbusplus::message_t& msg) {
            if (iaContainsProbeInterface(msg, probeInterfaces))
            {
                propertiesChangedCallback();
            }
        });

    interfacesRemovedMatch = std::make_unique<sdbusplus::match>(
        static_cast<sdbusplus::bus_t&>(*systemBus),
        sdbusplus::match_rules::interfacesRemoved(),
        [this, probeInterfaces](sdbusplus::message_t& msg) {
            auto [path, interfaces] =
                msg.unpack<sdbusplus::object_path, std::vector<std::string>>();

            if (irContainsProbeInterface(interfaces, probeInterfaces))
            {
                // Clean up match on probe interface removal to avoid leaks
                dbusMatches.erase(path);
                propertiesChangedCallback();
            }
        });
}
