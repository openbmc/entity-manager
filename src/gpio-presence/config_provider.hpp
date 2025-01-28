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

#include <boost/container/flat_map.hpp>
#include <gpiod.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>

#include <string>
#include <variant>
#include <vector>

using BasicVariantType =
    std::variant<std::vector<std::string>, std::string, int64_t, uint64_t,
                 double, int32_t, uint32_t, int16_t, uint16_t, uint8_t, bool>;
using SensorBaseConfigMap =
    boost::container::flat_map<std::string, BasicVariantType>;
using SensorBaseConfiguration = std::pair<std::string, SensorBaseConfigMap>;
using SensorData = boost::container::flat_map<std::string, SensorBaseConfigMap>;
using ManagedObjectType =
    boost::container::flat_map<sdbusplus::message::object_path, SensorData>;

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
    sdbusplus::async::task<void> interfacesAddedHandler(
        sdbusplus::message::message& msg,
        const std::function<void(const sdbusplus::message::object_path& obj,
                                 std::unique_ptr<Config> config)>&
            addConfigSingleDevice);

    sdbusplus::async::task<void> setupConfiguration(
        boost::system::error_code ec, const ManagedObjectType& managedObjs,
        const std::function<void(const sdbusplus::message::object_path& obj,
                                 std::unique_ptr<Config> config)>&
            addConfigSingleDevice);

    std::unique_ptr<sdbusplus::bus::match::match> ifcAdded;
    std::unique_ptr<sdbusplus::bus::match::match> ifcRemoved;

    sdbusplus::async::context& io;
};

} // namespace gpio_presence
