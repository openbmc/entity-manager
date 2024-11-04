/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "config.hpp"
#include "config_provider.hpp"
#include "sdbusplus/async/fdio.hpp"

#include <gpiod.hpp>

#include <string>
#include <unordered_map>

namespace gpio_presence
{

const std::string appName = "GPIOPresenceSensor";

const std::string service = "xyz.openbmc_project." + appName;

class GPIOPresence;

class GPIOPresence : public std::enable_shared_from_this<GPIOPresence>
{
  public:
    explicit GPIOPresence(sdbusplus::async::context& ctx);

    // @param[in] name       name of device, e.g. 'cable0'
    // @returns              true if present
    bool getPresence(const std::string& name);

    // @brief fetch configuration from dbus via object mapper
    // and register dbus matches for configuration
    sdbusplus::async::task<void> initialize();

    std::string setupBusName() const;

    // add the configuration for object at path 'obj'
    // @param[in] obj       object path for the new config
    // @param[in] config    configuration for the new object
    void addConfig(const sdbusplus::message::object_path& obj,
                   std::unique_ptr<Config> config);

    void addConfigFromDbus(const std::string& service,
                           const sdbusplus::message::object_path& obj);

    sdbusplus::async::task<void> addConfigFromDbusAsync(
        const std::string& service, const sdbusplus::message::object_path& obj);

    // deletes our configuration for the object at 'objPath'
    // @param[in] objPath         path of the object we want to forget
    void removeConfig(const std::string& objPath);

    // updates presence information based on new gpio state
    // @param[in] gpioLine       name of the gpio line
    // @param[in] state          new state of the gpio line
    void updatePresence(const std::string& gpioLine, bool state);

  private:
    sdbusplus::async::task<void> readGPIOAsyncEvent(std::string gpioLine);

    // maps dbus object paths to configurations
    // e.g. /xyz/openbmc_project/inventory/system/board/My_Baseboard/cable0
    std::unordered_map<std::string, std::unique_ptr<Config>> configs;

    // maps gpio names to fdios
    std::unordered_map<std::string, std::unique_ptr<sdbusplus::async::fdio>>
        fdios;
    // maps gpio names to libgpiod lines
    std::unordered_map<std::string, ::gpiod::line> gpioLines;

    std::unordered_map<std::string, DevicePresenceInterface> interfacesMap;

    sdbusplus::async::context& ctx;
    sdbusplus::server::manager_t manager;

    ConfigProvider configProvider;
};

} // namespace gpio_presence
