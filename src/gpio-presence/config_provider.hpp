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

    auto initialize(AddedCallback addConfig, RemovedCallback removeConfig)
        -> sdbusplus::async::task<void>;

  private:
    auto getConfig(AddedCallback addConfig) -> sdbusplus::async::task<void>;

    auto handleInterfacesAdded(AddedCallback addConfig)
        -> sdbusplus::async::task<void>;

    auto handleInterfacesRemoved(RemovedCallback removeConfig)
        -> sdbusplus::async::task<void>;

    // name of the dbus configuration interface
    std::string interface;

    sdbusplus::async::context& ctx;
};

} // namespace gpio_presence
