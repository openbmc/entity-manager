/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "config_provider.hpp"
#include "device_presence.hpp"
#include "sdbusplus/async/fdio.hpp"

#include <gpiod.hpp>

#include <string>
#include <unordered_map>

namespace gpio_presence
{

constexpr auto service = "xyz.openbmc_project.gpiopresence";

class GPIOPresenceManager
{
  public:
    explicit GPIOPresenceManager(sdbusplus::async::context& ctx);

    // spawns the initialization function
    auto start() -> void;

    // @param[in] name       name of device, e.g. 'cable0'
    // @returns              true if present
    auto getPresence(const std::string& name) -> bool;

    // request our dbus name
    // @returns         our dbus name
    auto setupBusName() const -> std::string;

    // add the configuration for object at path 'obj'
    // @param[in] obj       object path for the new config
    // @param[in] config    configuration for the new object
    auto addConfig(const sdbusplus::message::object_path& obj,
                   std::unique_ptr<DevicePresence> config) -> void;

    // update presence information based on new gpio state
    // @param[in] gpioLine       name of the gpio line
    // @param[in] state          new state of the gpio line
    auto updatePresence(const std::string& gpioLine, bool state) -> void;

    // maps gpio names to cached gpio state
    // true <=> High
    std::unordered_map<std::string, bool> gpioState;

  private:
    // fetch our configuration from dbus
    // @param[in] obj object path of the configuration
    auto addConfigFromDbusAsync(sdbusplus::message::object_path obj)
        -> sdbusplus::async::task<void>;

    // fetch the parent inventory items 'Compatible' Decorator
    // @param[in] obj object path of our configuration
    auto getParentInventoryCompatible(
        const sdbusplus::message::object_path& obj)
        -> sdbusplus::async::task<std::vector<std::string>>;

    // delete our configuration for the object at 'objPath'
    // @param[in] objPath         path of the object we want to forget
    auto removeConfig(const std::string& objPath) -> void;

    // fetch configuration from dbus via object mapper
    // and register dbus matches for configuration
    auto initialize() -> sdbusplus::async::task<void>;

    // handle config interface added
    // @param[in] obj    object path of the configuration
    auto addConfigHandler(sdbusplus::message::object_path obj) -> void;

    // async block on fdio gpio event and handle it
    // @param[in] gpioLine     name of the gpio to watch for events
    auto readGPIOAsyncEvent(std::string gpioLine)
        -> sdbusplus::async::task<void>;

    // maps dbus object paths to configurations
    // e.g. /xyz/openbmc_project/inventory/system/board/My_Baseboard/cable0
    std::unordered_map<std::string, std::unique_ptr<DevicePresence>>
        presenceMap;

    // maps gpio names to fdios
    std::unordered_map<std::string, std::unique_ptr<sdbusplus::async::fdio>>
        fdios;
    // maps gpio names to libgpiod lines
    std::unordered_map<std::string, ::gpiod::line> gpioLines;

    sdbusplus::async::context& ctx;
    sdbusplus::server::manager_t manager;

    ConfigProvider configProvider;
};

} // namespace gpio_presence
