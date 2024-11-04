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

#include "gpio_presence.hpp"

#include "config.hpp"

#include <boost/asio/posix/stream_descriptor.hpp>
#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/timer.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <memory>
#include <ranges>
#include <string>

PHOSPHOR_LOG2_USING;

namespace gpio_presence
{

bool GPIOPresence::getPresence(const std::string& name)
{
    if (!this->configs.contains(name))
    {
        return false;
    }
    return this->configs.at(name)->isPresent();
}

std::string GPIOPresence::setupBusName() const
{
    debug("requesting dbus name {NAME}", "NAME", service);

    ctx.request_name(service.c_str());
    return service;
}

void GPIOPresence::addConfigSingleDevice(
    const sdbusplus::message::object_path& obj, std::unique_ptr<Config> config)
{
    this->addConfig(obj.str, std::move(config));

    debug("found valid configuration at object path {OBJPATH}", "OBJPATH",
          obj.str);

    auto gpioConfigs = configs[obj.str]->gpioConfigs;

    // populate fdios
    for (auto& [_, gpioConfig] : gpioConfigs)
    {
        if (gpioLines.contains(gpioConfig.gpioLine))
        {
            continue;
        }

        try
        {
            gpioLines[gpioConfig.gpioLine] =
                gpiod::find_line(gpioConfig.gpioLine);
        }
        catch (std::exception& e)
        {
            error("gpiod::find_line failed: {ERROR}", "ERROR", e.what());
            return;
        }

        gpiod::line_request lineConfig;
        lineConfig.consumer = "gpio-presence";
        lineConfig.request_type = gpiod::line_request::EVENT_BOTH_EDGES |
                                  gpiod::line_request::DIRECTION_INPUT;

        int lineFd = -1;
        try
        {
            gpioLines[gpioConfig.gpioLine].request(lineConfig);

            lineFd = gpioLines[gpioConfig.gpioLine].event_get_fd();
        }
        catch (std::exception& e)
        {
            error(e.what());
            return;
        }
        if (lineFd < 0)
        {
            error("could not get event fd for gpio '{NAME}'", "NAME",
                  gpioConfig.gpioLine);
            return;
        }

        if (!fdios.contains(gpioConfig.gpioLine))
        {
            fdios.insert(
                {gpioConfig.gpioLine,
                 std::make_unique<sdbusplus::async::fdio>(ctx, lineFd)});

            ctx.spawn(readGPIOAsyncEvent(gpioConfig.gpioLine));
        }
    }
}

GPIOPresence::GPIOPresence(sdbusplus::async::context& ctx,
                           ConfigProvider& configProvider) :
    ctx(ctx), manager(ctx, "/"), configProvider(configProvider)
{}

void GPIOPresence::addConfig(const std::string& objPath,
                             std::unique_ptr<Config> config)
{
    debug("adding configuration for {NAME}", "NAME", objPath);
    this->configs.insert_or_assign(objPath, std::move(config));
}

void GPIOPresence::removeConfig(const std::string& objPath)
{
    if (!configs.contains(objPath))
    {
        return;
    }

    debug("erasing configuration for object path {OBJPATH}", "OBJPATH",
          objPath);
    configs.erase(objPath);

    std::set<std::string> gpiosNeeded;

    for (const auto& config : std::views::values(configs))
    {
        for (const auto& gpio : std::views::keys(config->gpioConfigs))
        {
            gpiosNeeded.insert(gpio);
        }
    }

    auto ks = std::views::keys(gpioLines);
    std::set<std::string> trackedGPIOs{ks.begin(), ks.end()};

    for (const auto& trackedGPIO : trackedGPIOs)
    {
        if (gpiosNeeded.contains(trackedGPIO))
        {
            continue;
        }

        gpioLines[trackedGPIO].release();

        gpioLines.erase(trackedGPIO);
        fdios.erase(fdios.find(trackedGPIO));
    }
}

void GPIOPresence::updatePresence(const std::string& gpioLine, bool state)
{
    for (const auto& config : std::views::values(configs))
    {
        config->updatePresence(gpioLine, state);
    }
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> GPIOPresence::readGPIOAsyncEvent(
    std::string gpioLine)
// NOLINTEND(readability-static-accessed-through-instance)
{
    debug("Watching gpio events for {LINENAME}", "LINENAME", gpioLine);

    if (!fdios.contains(gpioLine))
    {
        error("fdio for {LINENAME} not found", "LINENAME", gpioLine);
        co_return;
    }

    const auto& fdio = fdios[gpioLine];

    try
    {
        const int lineValue = gpioLines[gpioLine].get_value();

        updatePresence(gpioLine, lineValue == gpiod::line_event::RISING_EDGE);
    }
    catch (std::exception& e)
    {
        error("Failed to read GPIO line {LINENAME}", "LINENAME", gpioLine);
        error(e.what());
        co_return;
    }

    while (!ctx.stop_requested())
    {
        co_await fdio->next();

        debug("Received gpio event for {LINENAME}", "LINENAME", gpioLine);

        gpioLines[gpioLine].event_read();

        auto lineValue = gpioLines[gpioLine].get_value();

        if (lineValue < 0)
        {
            error("Failed to read GPIO line {LINENAME}", "LINENAME", gpioLine);
        }

        updatePresence(gpioLine, lineValue == gpiod::line_event::RISING_EDGE);
    }
}

} // namespace gpio_presence
