/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <sdbusplus/async/context.hpp>
#include <sdbusplus/async/task.hpp>

namespace gpio_presence
{

class GPIOPresence;

using ConfigAddCallback = std::function<void(
    const std::string& service, const sdbusplus::message::object_path&)>;

using ConfigRemoveCallback =
    std::function<void(const sdbusplus::message::object_path&)>;

class ConfigProvider
{
  public:
    explicit ConfigProvider(sdbusplus::async::context& ctx,
                            const std::string& interface);

    sdbusplus::async::task<> initialize(ConfigAddCallback addConfig,
                                        ConfigRemoveCallback removeConfig);

  private:
    sdbusplus::async::task<void> handleInterfacesAdded(
        ConfigAddCallback addConfig);

    sdbusplus::async::task<void> handleInterfacesRemoved(
        ConfigRemoveCallback removeConfig);

    // name of the dbus configuration interface
    std::string dbusInterface;

    sdbusplus::async::context& ctx;
};

} // namespace gpio_presence
