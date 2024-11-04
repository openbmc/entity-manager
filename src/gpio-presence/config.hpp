/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <xyz/openbmc_project/Inventory/Source/DevicePresence/aserver.hpp>

#include <string>

namespace gpio_presence
{

struct GpioConfig
{
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
        sdbusplus::async::context& ctx, const std::string& service,
        sdbusplus::message::object_path objPath);

    Config(sdbusplus::async::context& ctx,
           const std::vector<std::string>& gpioNames,
           const std::vector<uint64_t>& gpioValues, const std::string& name);

    void updatePresence(const std::string& gpioLine, bool state);

    // @returns the object path of the 'detected' interface
    sdbusplus::message::object_path getObjPath() const;

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
