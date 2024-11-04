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

#include <xyz/openbmc_project/Inventory/Source/DevicePresence/aserver.hpp>

#include <string>

namespace gpio_presence
{

struct GpioConfig
{
    std::string gpioLine;

    bool activeLow{};

    bool present{};
};

class Config;

using DevicePresenceInterface =
    sdbusplus::aserver::xyz::openbmc_project::inventory::source::DevicePresence<
        Config>;

class Config
{
  public:
    // @returns nullptr in case of misconfiguration
    static sdbusplus::async::task<std::unique_ptr<Config>> makeConfig(
        sdbusplus::async::context& ctx, std::string& service,
        sdbusplus::message::object_path objPath, const std::string& name);

    Config(sdbusplus::async::context& ctx,
           const std::vector<std::string>& gpioNames,
           const std::vector<uint64_t>& gpioValues, const std::string& name);

    // @returns        the object path of the 'detected' interface
    sdbusplus::message::object_path getObjPath() const;

    void updatePresence(const std::string& gpioLine, bool state);

    // computed from the state of the configured gpios
    bool isPresent();

    // name of the device to detect, e.g. 'cable0'
    // (taken from EM config)
    const std::string name;

    // maps the name of the gpio to its config
    std::map<std::string, GpioConfig> gpioConfigs;

  private:
    sdbusplus::async::context& ctx;

    void updateDbusInterfaces();

    // property added when the hw is detected
    std::unique_ptr<DevicePresenceInterface> detectedIface = nullptr;
};

} // namespace gpio_presence
