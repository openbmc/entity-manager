/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <xyz/openbmc_project/Inventory/Source/DevicePresence/aserver.hpp>

#include <string>

namespace gpio_presence
{

enum GPIO_POLARITY
{
    ACTIVE_HIGH,
    ACTIVE_LOW,
};

class DevicePresence;

using DevicePresenceInterface =
    sdbusplus::aserver::xyz::openbmc_project::inventory::source::DevicePresence<
        DevicePresence>;

class DevicePresence
{
  public:
    DevicePresence(sdbusplus::async::context& ctx,
                   const std::vector<std::string>& gpioNames,
                   const std::vector<uint64_t>& gpioValues,
                   const std::string& deviceName,
                   const std::unordered_map<std::string, bool>& gpioState);

    void updateGPIOPresence(const std::string& gpioLine);

    // @returns the object path of the 'detected' interface
    sdbusplus::message::object_path getObjPath() const;

    // computed from the state of the configured gpios
    bool isPresent();

    // name of the device to detect, e.g. 'cable0'
    // (taken from EM config)
    const std::string deviceName;

    // maps the name of the gpio to its polarity
    std::map<std::string, enum GPIO_POLARITY> gpioPolarity;

  private:
    // reference to the map in presence manager
    const std::unordered_map<std::string, bool>& gpioState;

    sdbusplus::async::context& ctx;

    void updateDbusInterfaces();

    // property added when the hw is detected
    std::unique_ptr<DevicePresenceInterface> detectedIface = nullptr;
};

} // namespace gpio_presence
