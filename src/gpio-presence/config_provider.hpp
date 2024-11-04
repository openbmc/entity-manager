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
#pragma once

#include "config.hpp"

#include <sdbusplus/bus/match.hpp>

namespace gpio_presence
{

class ConfigProvider : public std::enable_shared_from_this<ConfigProvider>
{
  public:
    explicit ConfigProvider(sdbusplus::async::context& io);

    sdbusplus::async::task<> initialize(
        sdbusplus::async::context& ctx,
        const std::function<void(const sdbusplus::message::object_path& path)>&
            removeHandler,
        const std::function<void(const sdbusplus::message::object_path& obj,
                                 std::unique_ptr<Config> config)>&
            addConfigSingleDevice);

  private:
    void interfacesAddedHandler(
        sdbusplus::message::message& msg,
        const std::function<void(const sdbusplus::message::object_path& obj,
                                 std::unique_ptr<Config> config)>&
            addConfigSingleDevice);

    void setupConfigurationSingleDevice(
        const sdbusplus::message::object_path& obj, const SensorData& item,
        const std::function<void(const sdbusplus::message::object_path& obj,
                                 std::unique_ptr<Config> config)>&
            addConfigSingleDevice);

    void setupConfiguration(
        boost::system::error_code ec, const ManagedObjectType& managedObjs,
        const std::function<void(const sdbusplus::message::object_path& obj,
                                 std::unique_ptr<Config> config)>&
            addConfigSingleDevice);

    std::unique_ptr<sdbusplus::bus::match::match> ifcAdded;
    std::unique_ptr<sdbusplus::bus::match::match> ifcRemoved;

    sdbusplus::async::context& io;
    // sdbusplus::bus_t& bus;
};

} // namespace gpio_presence
