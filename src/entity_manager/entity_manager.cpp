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

#include "../utils.hpp"
#include "../variant_visitors.hpp"
#include "configuration.hpp"
#include "dbus_interface.hpp"
#include "overlay.hpp"
#include "perform_scan.hpp"
#include "topology.hpp"

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

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)

// todo: pass this through nicer
std::shared_ptr<sdbusplus::asio::connection> systemBus;
nlohmann::json lastJson;
Topology topology;

boost::asio::io_context io;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

const std::regex illegalDbusPathRegex("[^A-Za-z0-9_.]");
const std::regex illegalDbusMemberRegex("[^A-Za-z0-9_]");

sdbusplus::asio::PropertyPermission getPermission(const std::string& interface)
{
    return std::find(settableInterfaces.begin(), settableInterfaces.end(),
                     interface) != settableInterfaces.end()
               ? sdbusplus::asio::PropertyPermission::readWrite
               : sdbusplus::asio::PropertyPermission::readOnly;
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
            dbus_interface::createInterface(objServer, boardPath,
                                       "xyz.openbmc_project.Inventory.Item",
                                       boardName);

        std::shared_ptr<sdbusplus::asio::dbus_interface> boardIface =
            dbus_interface::createInterface(
                objServer, boardPath,
                "xyz.openbmc_project.Inventory.Item." + boardType,
                boardNameOrig);

        dbus_interface::createAddObjectMethod(jsonPointerPath, boardPath,
                                         systemConfiguration, objServer,
                                         boardNameOrig);

        dbus_interface::populateInterfaceFromJson(
            systemConfiguration, jsonPointerPath, boardIface, boardValues,
            objServer);
        jsonPointerPath += "/";
        // iterate through board properties
        for (const auto& [propName, propValue] : boardValues.items())
        {
            if (propValue.type() == nlohmann::json::value_t::object)
            {
                std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
                    dbus_interface::createInterface(objServer, boardPath, propName,
                                               boardNameOrig);

                dbus_interface::populateInterfaceFromJson(
                    systemConfiguration, jsonPointerPath + propName, iface,
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
                    dbus_interface::createInterface(
                        objServer, ifacePath,
                        "xyz.openbmc_project.Inventory.Item.Bmc",
                        boardNameOrig);
                dbus_interface::populateInterfaceFromJson(
                    systemConfiguration, jsonPointerPath, bmcIface, item,
                    objServer, getPermission(itemType));
            }
            else if (itemType == "System")
            {
                std::shared_ptr<sdbusplus::asio::dbus_interface> systemIface =
                    dbus_interface::createInterface(
                        objServer, ifacePath,
                        "xyz.openbmc_project.Inventory.Item.System",
                        boardNameOrig);
                dbus_interface::populateInterfaceFromJson(
                    systemConfiguration, jsonPointerPath, systemIface, item,
                    objServer, getPermission(itemType));
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
                        objectIface = dbus_interface::createInterface(
                            objServer, ifacePath, ifaceName, boardNameOrig);

                    dbus_interface::populateInterfaceFromJson(
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
                            objectIface = dbus_interface::createInterface(
                                objServer, ifacePath, ifaceName, boardNameOrig);

                        dbus_interface::populateInterfaceFromJson(
                            systemConfiguration,
                            jsonPointerPath + "/" + std::to_string(index),
                            objectIface, arrayItem, objServer,
                            getPermission(name));
                        index++;
                    }
                }
            }

            std::shared_ptr<sdbusplus::asio::dbus_interface> itemIface =
                dbus_interface::createInterface(
                    objServer, ifacePath,
                    "xyz.openbmc_project.Configuration." + itemType,
                    boardNameOrig);

            dbus_interface::populateInterfaceFromJson(
                systemConfiguration, jsonPointerPath, itemIface, item,
                objServer, getPermission(itemType));

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

        auto ifacePtr = dbus_interface::createInterface(
            objServer, assocPath, "xyz.openbmc_project.Association.Definitions",
            findBoard->second);

        ifacePtr->register_property("Associations", assocPropValue);
        dbus_interface::tryIfaceInitialize(ifacePtr);
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

    auto& ifaces = dbus_interface::getDeviceInterfaces(device);
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
    dbus_interface::tryIfaceInitialize(entityIface);

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
