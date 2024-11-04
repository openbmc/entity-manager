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

auto GPIOPresenceManager::start() -> void
{
    ctx.spawn(initialize());
}

auto GPIOPresenceManager::getPresence(const std::string& name) -> bool
{
    if (!presenceMap.contains(name))
    {
        return false;
    }
    return presenceMap.at(name)->isPresent();
}

auto GPIOPresenceManager::initialize() -> sdbusplus::async::task<void>
{
    co_await configProvider.initialize(
        std::bind_front(&GPIOPresenceManager::addConfigHandler, this),
        std::bind_front(&GPIOPresenceManager::removeConfig, this));
}

auto GPIOPresenceManager::setupBusName() const -> std::string
{
    debug("requesting dbus name {NAME}", "NAME", service);

    ctx.request_name(service);
    return service;
}

auto GPIOPresenceManager::addConfig(const sdbusplus::message::object_path& obj,
                                    std::unique_ptr<DevicePresence> config)
    -> void
{
    debug("adding configuration for {NAME}", "NAME", obj);
    presenceMap.insert_or_assign(obj, std::move(config));

    debug("found valid configuration at object path {OBJPATH}", "OBJPATH", obj);

    auto gpioConfigs = presenceMap[obj]->gpioPolarity;

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
            error("gpiod::find_line failed: {ERROR}", "ERROR", e);
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
            error("{ERROR}", "ERROR", e);
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

auto GPIOPresenceManager::addConfigHandler(sdbusplus::message::object_path obj)
    -> void
{
    // NOLINTBEGIN(performance-unnecessary-value-param)
    ctx.spawn(addConfigFromDbusAsync(obj));
    // NOLINTEND(performance-unnecessary-value-param)
}

// NOLINTBEGIN(performance-unnecessary-value-param)
auto GPIOPresenceManager::addConfigFromDbusAsync(
    const sdbusplus::message::object_path obj) -> sdbusplus::async::task<void>
// NOLINTEND(performance-unnecessary-value-param)
{
    auto props = co_await sdbusplus::client::xyz::openbmc_project::
                     configuration::GPIODeviceDetect<>(ctx)
                         .service("xyz.openbmc_project.EntityManager")
                         .path(obj.str)
                         .properties();

    if (props.presence_pin_names.size() != props.presence_pin_values.size())
    {
        error(
            "presence pin names and presence pin values have different sizes");
        co_return;
    }

    auto devicePresence = std::make_unique<DevicePresence>(
        ctx, props.presence_pin_names, props.presence_pin_values, props.name,
        gpioState);

    if (devicePresence)
    {
        addConfig(obj, std::move(devicePresence));
    }
}

auto GPIOPresenceManager::removeConfig(const std::string& objPath) -> void
{
    if (!presenceMap.contains(objPath))
    {
        return;
    }

    debug("erasing configuration for object path {OBJPATH}", "OBJPATH",
          objPath);
    presenceMap.erase(objPath);

    std::set<std::string> gpiosNeeded;

    for (const auto& config : std::views::values(presenceMap))
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

auto GPIOPresenceManager::updatePresence(const std::string& gpioLine,
                                         bool state) -> void
{
    gpioState.insert_or_assign(gpioLine, state);

    debug("GPIO line {GPIO_NAME} went {GPIO_LEVEL}", "GPIO_NAME", gpioLine,
          "GPIO_LEVEL", (state) ? "high" : "low");

    for (const auto& config : std::views::values(presenceMap))
    {
        config->updateGPIOPresence(gpioLine);
    }
}

auto GPIOPresenceManager::readGPIOAsyncEvent(std::string gpioLine)
    -> sdbusplus::async::task<void>
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
        error("{ERROR}", "ERROR", e);
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
