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

#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

namespace gpio_presence
{

DevicePresence::DevicePresence(
    sdbusplus::async::context& ctx, const std::vector<std::string>& gpioNames,
    const std::vector<uint64_t>& gpioValues, const std::string& deviceName,
    const std::unordered_map<std::string, bool>& gpioState) :
    deviceName(deviceName), gpioState(gpioState), ctx(ctx)
{
    for (size_t i = 0; i < gpioNames.size(); i++)
    {
        enum GPIO_POLARITY polarity =
            (gpioValues[i] == 0) ? ACTIVE_LOW : ACTIVE_HIGH;
        gpioPolarity[gpioNames[i]] = polarity;
    }
}

void DevicePresence::updateGPIOPresence(const std::string& gpioLine)
{
    if (!gpioPolarity.contains(gpioLine))
    {
        return;
    }

    updateDbusInterfaces();
}

auto DevicePresence::getObjPath() const -> sdbusplus::message::object_path
{
    sdbusplus::message::object_path objPathBase(
        "/xyz/openbmc_project/GPIODeviceDetected/");
    sdbusplus::message::object_path objPath = objPathBase / deviceName;
    return objPath;
}

auto DevicePresence::isPresent() -> bool
{
    for (auto& [name, polarity] : gpioPolarity)
    {
        if (!gpioState.contains(name))
        {
            error("GPIO {NAME} not in cached state", "NAME", name);
            return false;
        }

        const bool state = gpioState.at(name);

        if (state && polarity == ACTIVE_LOW)
        {
            return false;
        }
        if (!state && polarity == ACTIVE_HIGH)
        {
            return false;
        }
    }

    return true;
}

void DevicePresence::updateDbusInterfaces()
{
    debug("Updating dbus interface for config {OBJPATH}", "OBJPATH",
          deviceName);

    const bool present = isPresent();
    sdbusplus::message::object_path objPath = getObjPath();

    if (present && !detectedIface)
    {
        info("Detected {NAME} as present, adding dbus interface", "NAME",
             deviceName);

        detectedIface =
            std::make_unique<DevicePresenceInterface>(ctx, objPath.str.c_str());

        detectedIface->name(deviceName);

        detectedIface->emit_added();
    }

    if (!present && detectedIface)
    {
        info("Detected {NAME} as absent, removing dbus interface", "NAME",
             deviceName);
        detectedIface->emit_removed();

        detectedIface.reset();
    }
}

} // namespace gpio_presence
