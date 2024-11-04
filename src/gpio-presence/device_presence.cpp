/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "device_presence.hpp"

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

DevicePresence::DevicePresence(sdbusplus::async::context& ctx,
                               const std::vector<std::string>& gpioNames,
                               const std::vector<uint64_t>& gpioValues,
                               const std::string& name) : name(name), ctx(ctx)
{
    for (size_t i = 0; i < gpioNames.size(); i++)
    {
        enum GPIO_POLARITY polarity =
            (gpioValues[i] == 0) ? ACTIVE_LOW : ACTIVE_HIGH;
        gpioPolarity[gpioNames[i]] = polarity;
    }
}

void DevicePresence::updateGPIOPresence(const std::string& gpioLine, bool state)
{
    if (!gpioPolarity.contains(gpioLine))
    {
        error("Not updating presence for unrecognized gpio {GPIO_NAME}",
              "GPIO_NAME", gpioLine);
        return;
    }

    const bool activeLow = gpioPolarity[gpioLine] == ACTIVE_LOW;

    const bool present = activeLow ? !state : state;

    if (gpioPresence.contains(gpioLine) && gpioPresence[gpioLine] == present)
    {
        return;
    }

    gpioPresence[gpioLine] = present;

    debug("GPIO line {GPIO_NAME} went {GPIO_LEVEL}", "GPIO_NAME", gpioLine,
          "GPIO_LEVEL", (state) ? "high" : "low");

    updateDbusInterfaces();
}

sdbusplus::message::object_path DevicePresence::getObjPath() const
{
    sdbusplus::message::object_path objPathBase(
        "/xyz/openbmc_project/GPIODeviceDetected/");
    sdbusplus::message::object_path objPath = objPathBase / name;
    return objPath;
}

bool DevicePresence::isPresent()
{
    const auto isPresentFn = [](const bool& x) { return x; };
    const auto& values = std::views::values(gpioPresence);

    return std::ranges::find_if_not(values.begin(), values.end(),
                                    isPresentFn) == values.end();
}

void DevicePresence::updateDbusInterfaces()
{
    debug("Updating dbus interface for config {OBJPATH}", "OBJPATH", name);

    const bool present = isPresent();
    sdbusplus::message::object_path objPath = getObjPath();

    if (present && !detectedIface)
    {
        info("Detected {NAME} as present, adding dbus interface", "NAME", name);

        detectedIface =
            std::make_unique<DevicePresenceInterface>(ctx, objPath.str.c_str());

        detectedIface->name(name);

        detectedIface->emit_added();
    }

    if (!present && detectedIface)
    {
        info("Detected {NAME} as absent, removing dbus interface", "NAME",
             name);
        detectedIface.reset();
    }
}

} // namespace gpio_presence
