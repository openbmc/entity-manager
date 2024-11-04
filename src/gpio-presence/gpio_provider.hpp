/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024.
 * All rights reserved. SPDX-License-Identifier: Apache-2.0
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

#include <boost/asio.hpp>
#include <gpiod.hpp>
#include <sdbusplus/async/context.hpp>

#include <string>
#include <unordered_map>

namespace gpio_presence
{

class IGPIOProvider
{
  public:
    virtual int readLine(const std::string& lineLabel) = 0;
};

class GPIOProvider : public IGPIOProvider
{
  public:
    explicit GPIOProvider(sdbusplus::async::context& io);

    // @param lineLabel    gpio name
    // @returns            1 gpio is high
    // @returns            0 gpio is low
    // @returns            < 0 error
    int readLine(const std::string& lineLabel) override;

  private:
    sdbusplus::async::context& io;

    void releaseLine(const std::string& lineLabel);

    static void lineRequest(const gpiod::line& line,
                            const gpiod::line_request& config);

    // @param label         gpio name
    // @returns             gpio line if found
    static gpiod::line findLine(const std::string& label);

    // @param lineLabel     gpio name
    // @returns             true on success
    bool addInputLine(const std::string& lineLabel);

    // this maps gpio names to gpio lines
    std::unordered_map<std::string, gpiod::line> gpioLines;
};

} // namespace gpio_presence
