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
#include "gpio_provider.hpp"
#include "sdbusplus/async/timer.hpp"

#include <boost/asio/posix/stream_descriptor.hpp>
#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <cerrno>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>

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
    lg2::debug("requesting dbus name {NAME}", "NAME", service);

    ctx.get_bus().request_name(service.c_str());
    return service;
}

void GPIOPresence::addConfigSingleDevice(
    const sdbusplus::message::object_path& obj, std::unique_ptr<Config> config)
{
    this->addConfig(obj.str, std::move(config));

    lg2::debug("found valid configuration at object path {OBJPATH}", "OBJPATH",
               obj.str);

    lg2::debug(
        "initially read of all configured gpios to check device presence");
    bool success = this->readPresentForAllConfigs();

    if (!success)
    {
        lg2::error("could not readPresent value of gpios");
        return;
    }
}

// NOLINTBEGIN
sdbusplus::async::task<> GPIOPresence::startup(sdbusplus::async::context& io,
                                               ConfigProvider& configProvider)
// NOLINTEND
{
    co_await configProvider.initialize(
        io, [this](const std::string& path) { removeObj(path); },
        [this](const sdbusplus::message::object_path& obj,
               std::unique_ptr<Config> config) {
            addConfigSingleDevice(obj, std::move(config));
        });

    co_return;
}

GPIOPresence::GPIOPresence(sdbusplus::async::context& io,
                           IGPIOProvider& provider,
                           ConfigProvider& configProvider) :
    ctx(io), manager(io, "/"), gpioProvider(provider),
    configProvider(configProvider)
{
    io.spawn(startup(io, configProvider));
}

void GPIOPresence::addConfig(const std::string& objStr,
                             std::unique_ptr<Config> config)
{
    lg2::debug("adding configuration for {NAME}", "NAME", objStr);
    this->configs.insert_or_assign(objStr, std::move(config));
}

void GPIOPresence::removeObj(const std::string& objPath)
{
    if (!configs.contains(objPath))
    {
        return;
    }

    lg2::debug("erasing configuration for object path {OBJPATH}", "OBJPATH",
               objPath);
    configs.erase(objPath);
}

void GPIOPresence::updatePresence(const std::string& gpioLine, bool state)
{
    for (auto& [objpath, config] : configs)
    {
        config->updatePresence(gpioLine, state);
    }
}

bool GPIOPresence::readPresentSingleGPIO(GpioConfig& config)
{
    int lineValue = 0;
    try
    {
        lineValue = gpioProvider.readLine(config.gpioLine);
        if (lineValue < 0)
        {
            lg2::error("Failed gpio line read {GPIO_NAME}", "GPIO_NAME",
                       config.gpioLine);
            return false;
        }
    }
    catch (const std::invalid_argument& e)
    {
        lg2::error("Failed gpio line read {GPIO_NAME} error is: {MSG}",
                   "GPIO_NAME", config.gpioLine, "MSG", e.what());
        return false;
    }
    catch (const std::runtime_error& e)
    {
        lg2::error("Failed gpio line read {GPIO_NAME} error is: {MSG}",
                   "GPIO_NAME", config.gpioLine, "MSG", e.what());
        return false;
    }

    updatePresence(config.gpioLine, lineValue == 1);

    return true;
}

bool GPIOPresence::readPresentForAllConfigs()
{
    for (auto& [objpath, config] : configs)
    {
        for (auto& [_, value] : config->gpioConfigs)
        {
            bool success = readPresentSingleGPIO(value);
            if (!success)
            {
                return false;
            }
        }
    }
    return true;
}

// NOLINTBEGIN
sdbusplus::async::task<> GPIOPresence::startGPIOEventMonitor()
// NOLINTEND
{
    lg2::debug("starting gpio event monitor loop");

    // clang-tidy gives some weird error here.
    // NOLINTBEGIN
    while (true)
    {
        readPresentForAllConfigs();
        co_await sdbusplus::async::sleep_for(ctx, std::chrono::seconds(5));
    }
    // NOLINTEND
    co_return;
}

} // namespace gpio_presence
