/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <sdbusplus/async/context.hpp>
#include <sdbusplus/async/task.hpp>

namespace gpio_presence
{

using AddedCallback = std::function<void(
    const std::string& service, const sdbusplus::message::object_path&)>;

using RemovedCallback =
    std::function<void(const sdbusplus::message::object_path&)>;

class ConfigProvider
{
  public:
    explicit ConfigProvider(sdbusplus::async::context& ctx,
                            const std::string& interface);

    sdbusplus::async::task<void> initialize(AddedCallback addConfig,
                                            RemovedCallback removeConfig);

  private:
    sdbusplus::async::task<void> getConfig(AddedCallback addConfig);

    sdbusplus::async::task<void> handleInterfacesAdded(AddedCallback addConfig);

    sdbusplus::async::task<void> handleInterfacesRemoved(
        RemovedCallback removeConfig);

    // name of the dbus configuration interface
    std::string interface;

    sdbusplus::async::context& ctx;
};

} // namespace gpio_presence
