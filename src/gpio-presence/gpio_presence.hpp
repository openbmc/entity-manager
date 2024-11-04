/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
    explicit GPIOPresence(sdbusplus::async::context& ctx,
                          ConfigProvider& configProvider);

    // adds a new configuration to our map
    // @param[in] objPath     object path for the new configuration
    // @param[in] config      new configuration
    void addConfig(const std::string& objPath, std::unique_ptr<Config> config);

    std::string setupBusName() const;

    // add the configuration for object at path 'obj'
    // @param[in] obj       object path for the new config
    // @param[in] config    configuration for the new object
    void addConfigSingleDevice(const sdbusplus::message::object_path& obj,
                               std::unique_ptr<Config> config);

    // deletes our configuration for the object at 'objPath'
    // @param[in] objPath         path of the object we want to forget
    void removeConfig(const std::string& objPath);

    // updates presence information based on new gpio state
    // @param[in] gpioLine       name of the gpio line
    // @param[in] state          new state of the gpio line
    void updatePresence(const std::string& gpioLine, bool state);

    // @param[in] name       name of device, e.g. 'cable0'
    // @returns              true if present
    bool getPresence(const std::string& name);

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

    ConfigProvider& configProvider;
};

} // namespace gpio_presence
