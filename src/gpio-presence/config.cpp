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

#include "config.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message/native_types.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/client.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/common.hpp>
#include <xyz/openbmc_project/Inventory/Source/DevicePresence/aserver.hpp>

#include <format>
#include <stdexcept>
#include <string>

namespace gpio_presence
{

void Config::updatePresence(const std::string& gpioLine, bool state)
{
    if (!this->gpioConfigs.contains(gpioLine))
    {
        lg2::error("not updating presence for unrecognized gpio {GPIO_NAME}",
                   "GPIO_NAME", gpioLine);
        return;
    }

    GpioConfig& gc = this->gpioConfigs[gpioLine];

    const bool present = gc.activeLow ? !state : state;

    if (present == gc.present)
    {
        return;
    }

    gc.present = present;

    lg2::debug(
        "gpioLine {GPIO_NAME} went {GPIO_LEVEL}, meaning {CONNECT_STATUS}",
        "GPIO_NAME", gc.gpioLine, "GPIO_LEVEL", (state) ? "high" : "low",
        "CONNECT_STATUS", (present) ? "connected" : "disconnected");

    this->updateDbusInterfaces();
}

void Config::updateDbusInterfaces()
{
    lg2::debug("updating dbus interface for config {OBJPATH}", "OBJPATH",
               this->name);

    const bool present = this->isPresent();
    sdbusplus::message::object_path objPath = this->getObjPath();

    if (present && !this->detectedIface)
    {
        lg2::info("detected {NAME} as present, adding dbus interface", "NAME",
                  this->name);

        // TODO(alex): defer emit here, to avoid the empty "Name" property being
        // shown on dbus. Did not find a way yet to make this work with the
        // aserver.hpp
        this->detectedIface = std::make_unique<DevicePresenceInterface>(
            this->ctx, objPath.str.c_str());

        this->detectedIface->name(this->name);

        this->detectedIface->emit_added();
    }

    if (!present && this->detectedIface)
    {
        lg2::info("detected {NAME} as absent, removing dbus interface", "NAME",
                  this->name);
        this->detectedIface.reset();
    }
}

sdbusplus::message::object_path Config::getObjPath() const
{
    assert(!this->name.empty());
    sdbusplus::message::object_path objPathBase(
        "/xyz/openbmc_project/GPIODeviceDetected/");
    sdbusplus::message::object_path objPath = objPathBase / this->name;
    return objPath;
}

Config::Config(sdbusplus::async::context& ctx, const std::string& name) :
    ctx(ctx), name(name)
{
    lg2::debug("construct config for {NAME}", "NAME", name);
}

// NOLINTBEGIN
sdbusplus::async::task<std::unique_ptr<Config>> Config::makeConfig(
    sdbusplus::async::context& ctx, std::string& service,
    sdbusplus::message::object_path objPath, const std::string& name)
// NOLINTEND
{
    auto client = sdbusplus::client::xyz::openbmc_project::configuration::
                      GPIODeviceDetect<>(ctx)
                          .service(service)
                          .path(objPath.str);

    // std::string configName = co_await client.name();
    auto presencePinNames = co_await client.presence_pin_names();
    auto presencePinValues = co_await client.presence_pin_values();

    auto c = std::make_unique<Config>(ctx, name);

    for (size_t i = 0; i < presencePinNames.size(); i++)
    {
        auto& pinName = presencePinNames[i];
        auto& pinValue = presencePinValues[i];
        GpioConfig gc;
        gc.gpioLine = pinName;
        gc.activeLow = pinValue == 0;
        c->gpioConfigs[pinName] = gc;
    }

    co_return c;
}

Config::Config(sdbusplus::async::context& ctx,
               std::vector<std::string> gpioNames,
               std::vector<uint64_t> gpioValues, const std::string& name) :
    ctx(ctx), name(name)
{
    for (size_t i = 0; i < gpioNames.size(); i++)
    {
        std::string& gpioName = gpioNames[i];
        const int gpioValue = gpioValues[i];

        GpioConfig gc;
        gc.gpioLine = gpioName;
        gc.activeLow = gpioValue == 0;

        gpioConfigs[gpioName] = gc;
    }
}

bool Config::isPresent()
{
    bool present = true;
    for (const auto& [key, value] : gpioConfigs)
    {
        (void)key;
        if (!value.present)
        {
            present = false;
        }
    }
    return present;
}

} // namespace gpio_presence
