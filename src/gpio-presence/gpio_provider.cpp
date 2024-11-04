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

#include "gpio_provider.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string>

using namespace gpio_presence;

GPIOProvider::GPIOProvider(sdbusplus::async::context& io) : io(io) {}

bool GPIOProvider::addInputLine(const std::string& lineLabel)
{
    if (gpioLines.find(lineLabel) != gpioLines.end())
    {
        return true;
    }

    gpiod::line line = findLine(lineLabel);

    if (!line)
    {
        lg2::error("Failed to find line {GPIO_NAME}", "GPIO_NAME", lineLabel);
        return false;
    }
    std::string service = "gpio-presence";
    lineRequest(line, {service, ::gpiod::line_request::DIRECTION_INPUT, 0});

    gpioLines[lineLabel] = line;

    return true;
}

int GPIOProvider::readLine(const std::string& lineLabel)
{
    if (gpioLines.find(lineLabel) == gpioLines.end())
    {
        bool success = addInputLine(lineLabel);
        if (!success)
        {
            return -1;
        }
    }
    const int result = gpioLines[lineLabel].get_value();

    releaseLine(lineLabel);

    return result;
}

void GPIOProvider::releaseLine(const std::string& lineLabel)
{
    if (gpioLines.find(lineLabel) == gpioLines.end())
    {
        return;
    }

    ::gpiod::line line = findLine(lineLabel);

    line.release();
    gpioLines.erase(lineLabel);
}

void GPIOProvider::lineRequest(const gpiod::line& line,
                               const gpiod::line_request& config)
{
    line.request(config);
}

gpiod::line GPIOProvider::findLine(const std::string& label)
{
    return ::gpiod::find_line(label);
}
