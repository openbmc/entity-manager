// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "entity_manager.hpp"

#include "../dbus_util.hpp"
#include "../utils.hpp"
#include "../variant_visitors.hpp"
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

#include <filesystem>
#include <flat_map>
#include <fstream>
#include <functional>
#include <map>
#include <regex>
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
    const std::filesystem::path& schemaDirectory) :
    systemBus(systemBus),
    objServer(sdbusplus::asio::object_server(systemBus, /*skipManager=*/true)),
    configuration(configurationDirectories, schemaDirectory),
    lastJson(nlohmann::json::object()),
    systemConfiguration(nlohmann::json::object()), io(io),
    dbus_interface(io, objServer, schemaDirectory), powerStatus(*systemBus),
    propertiesChangedTimer(io)
{
    // All other objects that EntityManager currently support are under the
    // inventory subtree.
    // See the discussion at
    // https://discord.com/channels/775381525260664832/1018929092009144380
    objServer.add_manager("/xyz/openbmc_project/inventory");

    entityIface = objServer.add_interface(emDbusPath, emDbusName);
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
    std::string jsonPointerPath = "/" + configId;
    // loop through newConfiguration, but use values from system
    // configuration to be able to modify via dbus later
    auto configValues = systemConfiguration[configId];
    auto findConfigType = configValues.find("Type");
    std::string configType;
    if (findConfigType != configValues.end() &&
        findConfigType->type() == nlohmann::json::value_t::string)
    {
        configType = dbus_util::sanitizeForDBusPathSegment(
            findConfigType->get<std::string>());
    }
    else
    {
        lg2::error("Unable to find type for {CONFIG} reverting to Chassis.",
                   "CONFIG", configName);
        configType = "Chassis";
    }

    lg2::debug("post {TYPE} '{NAME}' to DBus", "TYPE", configType, "NAME",
               configName);

    const sdbusplus::object_path objectPath =
        em_utils::buildInventorySystemPath(configName, configType);

    std::shared_ptr<sdbusplus::asio::dbus_interface> inventoryIface =
        dbus_interface.createInterface(
            objectPath,
            sdbusplus::common::xyz::openbmc_project::inventory::Item::interface,
            configNameOrig);

    dbus_interface.createAddObjectMethod(jsonPointerPath, objectPath,
                                         systemConfiguration, configNameOrig);

    std::string jsonPointerPath1 = jsonPointerPath;
    jsonPointerPath += "/";

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
        if (propValue.type() == nlohmann::json::value_t::object)
        {
            std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
                dbus_interface.createInterface(objectPath, propName,
                                               configNameOrig);
            if (propName == invItemIntf)
            {
                typeIface = iface;
            }

            dbus_interface.populateInterfaceFromJson(
                systemConfiguration, jsonPointerPath + propName, iface,
                propValue);
        }
    }

    if (typeIface == nullptr && !invItemIntf.empty())
    {
        typeIface = dbus_interface.createInterface(objectPath, invItemIntf,
                                                   configNameOrig);
        // Name, Type and Probe say how the configuration was matched rather
        // than what the object is, and are published on Configuration.Probe.
        nlohmann::json itemValues = configValues;
        itemValues.erase("Name");
        itemValues.erase("Type");
        itemValues.erase("Probe");
        dbus_interface.populateInterfaceFromJson(
            systemConfiguration, jsonPointerPath1, typeIface, itemValues);
    }

    nlohmann::json::iterator exposes = configValues.find("Exposes");
    if (exposes == configValues.end())
    {
        return;
    }
    // iterate through exposes
    jsonPointerPath += "Exposes/";

    // store the configuration level pointer so we can modify it on the way down
    std::string jsonPointerPathConfig = jsonPointerPath;
    size_t exposesIndex = -1;
    for (nlohmann::json& item : *exposes)
    {
        postExposesRecordsToDBus(item, exposesIndex, configNameOrig,
                                 jsonPointerPath, jsonPointerPathConfig,
                                 objectPath, configType);
    }

    newObjects.emplace(objectPath, configNameOrig);
}

void EntityManager::postExposesRecordsToDBus(
    nlohmann::json& item, size_t& exposesIndex,
    const std::string& configNameOrig, std::string jsonPointerPath,
    const std::string& jsonPointerPathConfig,
    const sdbusplus::object_path& objectPath, const std::string& configType)
{
    exposesIndex++;
    jsonPointerPath = jsonPointerPathConfig;
    jsonPointerPath += std::to_string(exposesIndex);

    auto findName = item.find("Name");
    if (findName == item.end())
    {
        lg2::error("cannot find name in field {ITEM}", "ITEM", item);
        return;
    }
    auto findStatus = item.find("Status");
    // if status is not found it is assumed to be status = 'okay'
    if (findStatus != item.end())
    {
        if (*findStatus == "disabled")
        {
            return;
        }
    }
    auto findType = item.find("Type");
    std::string itemType = "unknown";
    if (findType != item.end())
    {
        itemType = findType->get<std::string>();
    }

    if (!dbus_util::validateDBusInterfaceSegments(itemType))
    {
        lg2::error(
            "item Type '{TYPE}' is not a valid D-Bus interface segment(s)",
            "TYPE", itemType);
        return;
    }

    const std::string itemName =
        dbus_util::sanitizeForDBusPathSegment(findName->get<std::string>());

    const sdbusplus::object_path ifacePath = objectPath / itemName;

    if (itemType == "BMC")
    {
        std::shared_ptr<sdbusplus::asio::dbus_interface> bmcIface =
            dbus_interface.createInterface(
                ifacePath,
                sdbusplus::common::xyz::openbmc_project::inventory::item::Bmc::
                    interface,
                configNameOrig);
        dbus_interface.populateInterfaceFromJson(
            systemConfiguration, jsonPointerPath, bmcIface, item,
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
            systemConfiguration, jsonPointerPath, systemIface, item,
            getPermission(itemType));
    }

    for (const auto& [name, config] : item.items())
    {
        jsonPointerPath = jsonPointerPathConfig;
        jsonPointerPath.append(std::to_string(exposesIndex))
            .append("/")
            .append(name);

        if (!postConfigurationRecord(name, config, configNameOrig, itemType,
                                     jsonPointerPath, ifacePath))
        {
            break;
        }
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> itemIface =
        dbus_interface.createInterface(
            ifacePath, "xyz.openbmc_project.Configuration." + itemType,
            configNameOrig);

    dbus_interface.populateInterfaceFromJson(
        systemConfiguration, jsonPointerPath, itemIface, item,
        getPermission(itemType));

    topology.addBoard(objectPath, configType, configNameOrig, item);
}

bool EntityManager::postConfigurationRecord(
    const std::string& name, nlohmann::json& config,
    const std::string& configNameOrig, const std::string& itemType,
    const std::string& jsonPointerPath, const sdbusplus::object_path& ifacePath)
{
    if (config.type() == nlohmann::json::value_t::object)
    {
        std::string ifaceName = "xyz.openbmc_project.Configuration.";
        ifaceName.append(itemType).append(".").append(name);

        std::shared_ptr<sdbusplus::asio::dbus_interface> objectIface =
            dbus_interface.createInterface(ifacePath, ifaceName,
                                           configNameOrig);

        dbus_interface.populateInterfaceFromJson(
            systemConfiguration, jsonPointerPath, objectIface, config,
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

        for (auto& arrayItem : config)
        {
            std::string ifaceName = "xyz.openbmc_project.Configuration.";
            ifaceName.append(itemType).append(".").append(name);
            ifaceName.append(std::to_string(index));

            std::shared_ptr<sdbusplus::asio::dbus_interface> objectIface =
                dbus_interface.createInterface(ifacePath, ifaceName,
                                               configNameOrig);

            dbus_interface.populateInterfaceFromJson(
                systemConfiguration,
                jsonPointerPath + "/" + std::to_string(index), objectIface,
                arrayItem, getPermission(name));
            index++;
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
            pruneDevice(systemConfiguration, powerOff, scannedPowerOff, name,
                        device);
        }

        scannedPowerOff = true;
        if (!powerOff)
        {
            scannedPowerOn = true;
        }
    });
}

void EntityManager::pruneConfiguration(bool powerOff, const std::string& name,
                                       const nlohmann::json& device)
{
    lg2::debug("pruning configuration");

    if (powerOff && deviceRequiresPowerOn(device))
    {
        // power not on yet, don't know if it's there or not
        return;
    }

    auto& ifaces = dbus_interface.getDeviceInterfaces(device);
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
        if (!writeJsonFiles(systemConfiguration))
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
            bool powerOff = !powerStatus.isPowerOn();
            for (const auto& [name, device] : missingConfigurations->items())
            {
                pruneConfiguration(powerOff, name, device);
            }
            nlohmann::json newConfiguration = systemConfiguration;

            deriveNewConfiguration(oldConfiguration, newConfiguration);

            for (const auto& [_, device] : newConfiguration.items())
            {
                logDeviceAdded(device);
            }

            propertiesChangedInProgress = false;

            boost::asio::post(io, [this, newConfiguration, count] {
                publishNewConfiguration(std::ref(propertiesChangedInstance),
                                        count, std::ref(propertiesChangedTimer),
                                        newConfiguration);
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
    if (EM_CACHE_CONFIGURATION && em_utils::fwVersionIsSame())
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
                    lg2::error("syntax error in {PATH}", "PATH",
                               lastConfiguration);
                }
                else
                {
                    lastJson = std::move(data);
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
        std::filesystem::remove(currentConfiguration, ec);
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
