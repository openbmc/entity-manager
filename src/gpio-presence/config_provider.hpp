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

#include <boost/container/flat_map.hpp>
#include <gpiod.hpp>
#include <sdbusplus/async/context.hpp>
#include <sdbusplus/async/match.hpp>
#include <sdbusplus/async/task.hpp>
#include <sdbusplus/bus/match.hpp>

#include <string>
#include <variant>
#include <vector>

using BasicVariantType =
    std::variant<std::vector<std::string>, std::string, int64_t, uint64_t,
                 double, int32_t, uint32_t, int16_t, uint16_t, uint8_t, bool>;
using SensorBaseConfigMap =
    boost::container::flat_map<std::string, BasicVariantType>;
using SensorData = boost::container::flat_map<std::string, SensorBaseConfigMap>;

namespace gpio_presence
{

class GPIOPresence;

class ConfigProvider
{
  public:
    explicit ConfigProvider(sdbusplus::async::context& ctx);

    sdbusplus::async::task<> initialize(
        sdbusplus::async::context& ctx,
        std::unique_ptr<GPIOPresence>& gpioPresence);

  private:
    sdbusplus::async::task<void> handleInterfacesAdded(
        std::unique_ptr<GPIOPresence>& gpioPresence);

    sdbusplus::async::task<void> handleInterfacesRemoved(
        std::unique_ptr<GPIOPresence>& gpioPresence);

    sdbusplus::async::context& ctx;
};

} // namespace gpio_presence
