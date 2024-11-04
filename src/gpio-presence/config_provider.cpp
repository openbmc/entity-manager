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
#include "config_provider.hpp"

#include "gpio_presence.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/client.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/common.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <string>

PHOSPHOR_LOG2_USING;

namespace gpio_presence
{

ConfigProvider::ConfigProvider(sdbusplus::async::context& ctx) : ctx(ctx) {}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<> ConfigProvider::initialize(
    sdbusplus::async::context& ctx, std::unique_ptr<GPIOPresence>& gpioPresence)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(ctx)
                      .service("xyz.openbmc_project.ObjectMapper")
                      .path("/xyz/openbmc_project/object_mapper");

    std::string intf = sdbusplus::common::xyz::openbmc_project::configuration::
        GPIODeviceDetect::interface;

    debug("calling 'GetSubTree' to find instances of {INTF}", "INTF", intf);

    using SubTreeType =
        std::map<std::string, std::map<std::string, std::vector<std::string>>>;
    SubTreeType res = {};

    try
    {
        res = co_await client.get_sub_tree("/xyz/openbmc_project/inventory", 0,
                                           {intf});
    }
    catch (std::exception& e)
    {
        error("Failed GetSubTree call for configuration interface: {ERR}",
              "ERR", e.what());
        co_return;
    }

    // call the user callback for all the device that is already available
    for (auto& [path, mapServiceInterfaces] : res)
    {
        for (const auto& [service, _] : mapServiceInterfaces)
        {
            debug("found configuration interface at {SERVICE} {PATH} {INTF}",
                  "SERVICE", service, "PATH", path, "INTF", intf);
            auto configClient = sdbusplus::client::xyz::openbmc_project::
                                    configuration::GPIODeviceDetect<>(ctx)
                                        .service(service)
                                        .path(path);

            std::string name = co_await configClient.name();
            std::vector<std::string> presenceGpioNames =
                co_await configClient.presence_pin_names();
            std::vector<uint64_t> presenceGpioValues =
                co_await configClient.presence_pin_values();

            auto config = std::make_unique<Config>(ctx, presenceGpioNames,
                                                   presenceGpioValues, name);
            gpioPresence->addConfigSingleDevice(path, std::move(config));
        }
    }

    debug("setting up dbus match for interfaces added");

    ctx.spawn(handleInterfacesAdded(gpioPresence));

    debug("setting up dbus match for interfaces removed");

    ctx.spawn(handleInterfacesRemoved(gpioPresence));

    debug("Initialization completed");

    co_return;
}

const auto matchRuleSenderEM =
    sdbusplus::bus::match::rules::sender("xyz.openbmc_project.EntityManager");

const auto matchRuleGPIODeviceDetectIface =
    sdbusplus::bus::match::rules::interface(
        sdbusplus::common::xyz::openbmc_project::configuration::
            GPIODeviceDetect::interface);

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> ConfigProvider::handleInterfacesAdded(
    std::unique_ptr<GPIOPresence>& gpioPresence)
// NOLINTEND(readability-static-accessed-through-instance)
{
    sdbusplus::async::match ifcAddedMatch(
        ctx, sdbusplus::bus::match::rules::interfacesAdded() +
                 matchRuleSenderEM + matchRuleGPIODeviceDetectIface);

    while (!ctx.stop_requested())
    {
        auto [objPath, serviceIntfMap] =
            co_await ifcAddedMatch
                .next<sdbusplus::message::object_path, SensorData>();

        debug("Called InterfacesAdded handler");

        debug("Detected interface added on {OBJPATH}", "OBJPATH", objPath);

        try
        {
            if (serviceIntfMap.empty())
            {
                co_return;
            }

            auto service = serviceIntfMap.begin()->first;

            int index = objPath.str.rfind('/');
            std::string name = objPath.str.substr(index + 1);

            auto config =
                co_await Config::makeConfig(ctx, service, objPath, name);

            gpioPresence->addConfigSingleDevice(objPath, std::move(config));
        }
        catch (std::exception& e)
        {
            error("Incomplete config found: {ERR}", "ERR", e.what());
        }
    }
};

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> ConfigProvider::handleInterfacesRemoved(
    std::unique_ptr<GPIOPresence>& gpioPresence)
// NOLINTEND(readability-static-accessed-through-instance)
{
    sdbusplus::async::match ifcRemovedMatch(
        ctx, sdbusplus::bus::match::rules::interfacesRemoved() +
                 matchRuleSenderEM + matchRuleGPIODeviceDetectIface);

    while (!ctx.stop_requested())
    {
        auto [objectPath, msg] =
            co_await ifcRemovedMatch.next<sdbusplus::message::object_path,
                                          std::vector<std::string>>();

        debug("Called InterfacesRemoved handler");

        debug("Detected interface removed on {OBJPATH}", "OBJPATH", objectPath);

        gpioPresence->removeConfig(objectPath);
    }
};

} // namespace gpio_presence
