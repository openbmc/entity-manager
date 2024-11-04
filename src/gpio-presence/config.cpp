/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "config.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message/native_types.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/client.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/common.hpp>
#include <xyz/openbmc_project/Inventory/Source/DevicePresence/aserver.hpp>

#include <ranges>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace gpio_presence
{

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<std::unique_ptr<Config>> Config::makeConfig(
    sdbusplus::async::context& ctx, const std::string& service,
    sdbusplus::message::object_path objPath)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::client::xyz::openbmc_project::configuration::
                      GPIODeviceDetect<>(ctx)
                          .service(service)
                          .path(objPath.str);

    auto presencePinNames = co_await client.presence_pin_names();
    auto presencePinValues = co_await client.presence_pin_values();

    if (presencePinNames.size() != presencePinValues.size())
    {
        error(
            "presence pin names and presence pin values have different sizes");
        co_return nullptr;
    }

    co_return std::make_unique<Config>(ctx, presencePinNames, presencePinValues,
                                       co_await client.name());
}

Config::Config(sdbusplus::async::context& ctx,
               const std::vector<std::string>& gpioNames,
               const std::vector<uint64_t>& gpioValues,
               const std::string& name) : name(name), ctx(ctx)
{
    for (size_t i = 0; i < gpioNames.size(); i++)
    {
        gpioConfigs[gpioNames[i]] =
            GpioConfig{.activeLow = (gpioValues[i] == 0)};
    }
}

void Config::updatePresence(const std::string& gpioLine, bool state)
{
    if (!this->gpioConfigs.contains(gpioLine))
    {
        error("Not updating presence for unrecognized gpio {GPIO_NAME}",
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

    debug("GPIO line {GPIO_NAME} went {GPIO_LEVEL}", "GPIO_NAME", gpioLine,
          "GPIO_LEVEL", (state) ? "high" : "low");

    this->updateDbusInterfaces();
}

sdbusplus::message::object_path Config::getObjPath() const
{
    sdbusplus::message::object_path objPathBase(
        "/xyz/openbmc_project/GPIODeviceDetected/");
    sdbusplus::message::object_path objPath = objPathBase / this->name;
    return objPath;
}

bool Config::isPresent()
{
    const auto isPresentFn = [](const GpioConfig& x) { return x.present; };
    const auto& values = std::views::values(gpioConfigs);

    return std::ranges::find_if_not(values.begin(), values.end(),
                                    isPresentFn) == values.end();
}

void Config::updateDbusInterfaces()
{
    debug("Updating dbus interface for config {OBJPATH}", "OBJPATH",
          this->name);

    const bool present = this->isPresent();
    sdbusplus::message::object_path objPath = this->getObjPath();

    if (present && !this->detectedIface)
    {
        info("Detected {NAME} as present, adding dbus interface", "NAME",
             this->name);

        this->detectedIface = std::make_unique<DevicePresenceInterface>(
            this->ctx, objPath.str.c_str());

        this->detectedIface->name(this->name);

        this->detectedIface->emit_added();
    }

    if (!present && this->detectedIface)
    {
        info("Detected {NAME} as absent, removing dbus interface", "NAME",
             this->name);
        this->detectedIface.reset();
    }
}

} // namespace gpio_presence
