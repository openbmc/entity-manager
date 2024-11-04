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

#include "interfaces.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message/native_types.hpp>

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
        // shown
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

static std::string getGpioInterface(unsigned int n)
{
    return std::format(
        "{}{}", interfaces::configurationGPIOHWDetectPresenceGpioInterface, n);
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

Config::Config(sdbusplus::async::context& ctx,
               const sdbusplus::message::object_path& obj,
               const SensorData& item, const std::string& name) :
    ctx(ctx), name(name)
{
    (void)obj;
    auto found = item.find(interfaces::emConfigurationGPIOHWDetectInterface);

    // configure gpios
    for (unsigned int i = 0; true; i++)
    {
        GpioConfig gc;

        found = item.find(getGpioInterface(i));

        if (found == item.end())
        {
            if (i > 0)
            {
                // we already have 1 gpio
                break;
            }
            lg2::error("could not find gpio interface for device {NAME}",
                       "NAME", name);
            throw std::invalid_argument("did not find gpio interface");
        }

        const SensorBaseConfigMap& gpioProperties = found->second;

        auto gpioName =
            loadVariant<std::string>(gpioProperties, properties::propertyName);
        auto polarity = loadVariant<std::string>(gpioProperties,
                                                 properties::propertyPolarity);

        gc.gpioLine = gpioName;
        gc.activeLow = polarity == "Low";

        gpioConfigs[gpioName] = gc;
    }
}

Config::Config(sdbusplus::async::context& ctx,
               const sdbusplus::message::object_path& obj,
               std::vector<std::string> gpioNames,
               std::vector<uint64_t> gpioValues, const std::string& name) :
    ctx(ctx), name(name)
{
    (void)obj;
    for (size_t i = 0; i < gpioNames.size(); i++)
    {
        // TODO: check
        std::string& gpioName = gpioNames[i];
        int gpioValue = gpioValues[i];

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
