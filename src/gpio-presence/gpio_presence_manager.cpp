/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024.
 * All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "gpio_presence_manager.hpp"

#include "device_presence.hpp"

#include <boost/asio/posix/stream_descriptor.hpp>
#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/timer.hpp>
#include <sdbusplus/message/native_types.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/client.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/common.hpp>

#include <memory>
#include <ranges>
#include <string>
#include <utility>

PHOSPHOR_LOG2_USING;

namespace gpio_presence
{

GPIOPresenceManager::GPIOPresenceManager(sdbusplus::async::context& ctx) :
    ctx(ctx), manager(ctx, "/"),
    configProvider(
        ConfigProvider(ctx, sdbusplus::common::xyz::openbmc_project::
                                configuration::GPIODeviceDetect::interface))
{}

bool GPIOPresenceManager::getPresence(const std::string& name)
{
    if (!configs.contains(name))
    {
        return false;
    }
    return configs.at(name)->isPresent();
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> GPIOPresenceManager::initialize()
// NOLINTEND(readability-static-accessed-through-instance)
{
    co_await configProvider.initialize(
        std::bind_front(&GPIOPresenceManager::addConfigFromDbus, this),
        std::bind_front(&GPIOPresenceManager::removeConfig, this));
}

std::string GPIOPresenceManager::setupBusName() const
{
    debug("requesting dbus name {NAME}", "NAME", service);

    ctx.request_name(service.c_str());
    return service;
}

void GPIOPresenceManager::addConfig(const sdbusplus::message::object_path& obj,
                                    std::unique_ptr<DevicePresence> config)
{
    debug("adding configuration for {NAME}", "NAME", obj);
    configs.insert_or_assign(obj, std::move(config));

    debug("found valid configuration at object path {OBJPATH}", "OBJPATH",
          obj.str);

    auto gpioConfigs = configs[obj.str]->gpioPolarity;

    // populate fdios
    for (auto& [gpioName, _] : gpioConfigs)
    {
        if (gpioLines.contains(gpioName))
        {
            continue;
        }

        try
        {
            gpioLines[gpioName] = gpiod::find_line(gpioName);
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
            gpioLines[gpioName].request(lineConfig);

            lineFd = gpioLines[gpioName].event_get_fd();
        }
        catch (std::exception& e)
        {
            error(e.what());
            return;
        }
        if (lineFd < 0)
        {
            error("could not get event fd for gpio '{NAME}'", "NAME", gpioName);
            return;
        }

        if (!fdios.contains(gpioName))
        {
            fdios.insert(
                {gpioName,
                 std::make_unique<sdbusplus::async::fdio>(ctx, lineFd)});

            ctx.spawn(readGPIOAsyncEvent(gpioName));
        }
    }
}

void GPIOPresenceManager::addConfigFromDbus(std::string service,
                                            sdbusplus::message::object_path obj)
{
    // NOLINTBEGIN(performance-unnecessary-value-param)
    ctx.spawn(addConfigFromDbusAsync(service, obj));
    // NOLINTEND(performance-unnecessary-value-param)
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
// NOLINTBEGIN(performance-unnecessary-value-param)
sdbusplus::async::task<void> GPIOPresenceManager::addConfigFromDbusAsync(
    const std::string service, const sdbusplus::message::object_path obj)
// NOLINTEND(performance-unnecessary-value-param)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::client::xyz::openbmc_project::configuration::
                      GPIODeviceDetect<>(ctx)
                          .service(service)
                          .path(obj.str);

    auto presencePinNames = co_await client.presence_pin_names();
    auto presencePinValues = co_await client.presence_pin_values();

    if (presencePinNames.size() != presencePinValues.size())
    {
        error(
            "presence pin names and presence pin values have different sizes");
        co_return;
    }

    const std::string name = co_await client.name();

    auto config = std::make_unique<DevicePresence>(ctx, presencePinNames,
                                                   presencePinValues, name);

    if (config)
    {
        addConfig(obj, std::move(config));
    }
}

void GPIOPresenceManager::removeConfig(const std::string& objPath)
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
        for (const auto& gpio : std::views::keys(config->gpioPolarity))
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

void GPIOPresenceManager::updatePresence(const std::string& gpioLine,
                                         bool state)
{
    for (const auto& config : std::views::values(configs))
    {
        config->updateGPIOPresence(gpioLine, state);
    }
}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> GPIOPresenceManager::readGPIOAsyncEvent(
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
