/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "device_presence.hpp"

#include <phosphor-logging/commit.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message/native_types.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/client.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/common.hpp>
#include <xyz/openbmc_project/Inventory/Source/DevicePresence/aserver.hpp>
#include <xyz/openbmc_project/State/Presence/event.hpp>

#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

using DevicePresenceProperties = sdbusplus::common::xyz::openbmc_project::
    inventory::source::DevicePresence::properties_t;

namespace gpio_presence
{

DevicePresence::DevicePresence(
    sdbusplus::async::context& ctx, const std::vector<std::string>& gpioNames,
    const std::vector<uint64_t>& gpioValues, const std::string& deviceName,
    const std::unordered_map<std::string, bool>& gpioState,
    const std::vector<std::string>& parentInvCompatible,
    bool logPresenceChangeIn) :
    deviceName(deviceName), gpioState(gpioState), ctx(ctx),
    parentInventoryCompatible(parentInvCompatible),
    logPresenceChange(logPresenceChangeIn)
{
    for (size_t i = 0; i < gpioNames.size(); i++)
    {
        GPIO_POLARITY polarity =
            (gpioValues[i] == 0) ? ACTIVE_LOW : ACTIVE_HIGH;
        gpioPolarity[gpioNames[i]] = polarity;
    }
}

auto DevicePresence::updateGPIOPresence(const std::string& gpioLine) -> void
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

auto DevicePresence::updateDbusInterfaces() -> void
{
    debug("Updating dbus interface for config {OBJPATH}", "OBJPATH",
          deviceName);

    const bool present = isPresent();
    sdbusplus::message::object_path objPath = getObjPath();

    if (present && !detectedIface)
    {
        info("Detected {NAME} as present, adding dbus interface", "NAME",
             deviceName);

        const std::string firstCompatible =
            parentInventoryCompatible.empty() ? ""
                                              : parentInventoryCompatible[0];

        detectedIface = std::make_unique<DevicePresenceInterface>(
            ctx, objPath.str.c_str(),
            DevicePresenceProperties{deviceName, firstCompatible});

        detectedIface->emit_added();

        if (logPresenceChange)
        {
            auto logPath = lg2::commit(
                sdbusplus::event::xyz::openbmc_project::state::Presence::
                    DevicePresent("DEVICE_NAME", deviceName));

            info("Created present event log for {NAME} at {PATH}", "NAME",
                 deviceName, "PATH", logPath.str);
        }
    }

    if (!present && detectedIface)
    {
        info("Detected {NAME} as absent, removing dbus interface", "NAME",
             deviceName);
        detectedIface->emit_removed();

        detectedIface.reset();

        if (logPresenceChange)
        {
            auto logPath = lg2::commit(
                sdbusplus::event::xyz::openbmc_project::state::Presence::
                    DeviceAbsent("DEVICE_NAME", deviceName));

            info("Created absent event log for {NAME} at {PATH}", "NAME",
                 deviceName, "PATH", logPath.str);
        }
    }
}

} // namespace gpio_presence
