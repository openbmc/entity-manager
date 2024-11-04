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

namespace gpio_presence
{

ConfigProvider::ConfigProvider(sdbusplus::async::context& io) : io(io) {}

// NOLINTBEGIN
sdbusplus::async::task<void> ConfigProvider::interfacesAddedHandler(
    sdbusplus::message::message& msg,
    std::unique_ptr<GPIOPresence>& gpioPresence)
// NOLINTEND
{
    sdbusplus::message::object_path objPath;
    SensorData serviceIntfMap;
    msg.read(objPath, serviceIntfMap);

    if (serviceIntfMap.empty())
    {
        co_return;
    }

    auto service = serviceIntfMap.begin()->first;

    try
    {
        int index = objPath.str.rfind('/');
        std::string name = objPath.str.substr(index + 1);

        auto config = co_await Config::makeConfig(io, service, objPath, name);

        gpioPresence->addConfigSingleDevice(objPath, std::move(config));
    }
    catch (std::exception& e)
    {
        lg2::error("Incomplete config found: {ERR} obj = {OBJPATH}", "ERR",
                   e.what(), "OBJPATH", objPath.str);
        co_return;
    }
}

// NOLINTBEGIN
sdbusplus::async::task<> ConfigProvider::initialize(
    sdbusplus::async::context& ctx, std::unique_ptr<GPIOPresence>& gpioPresence)
// NOLINTEND
{
    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(ctx)
                      .service("xyz.openbmc_project.ObjectMapper")
                      .path("/xyz/openbmc_project/object_mapper");

    std::string intf = sdbusplus::common::xyz::openbmc_project::configuration::
        GPIODeviceDetect::interface;

    lg2::debug("calling 'GetSubTree' to find instances of {INTF}", "INTF",
               intf);

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
        lg2::error(e.what());
        co_return;
    }
    catch (...)
    {
        co_return;
    }

    // call the user callback for all the device that is already available
    for (auto& [path, mapServiceInterfaces] : res)
    {
        for (const auto& [service, _] : mapServiceInterfaces)
        {
            lg2::debug(
                "found configuration interface at {SERVICE} {PATH} {INTF}",
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

    lg2::debug("setting up dbus match for interfaces added");

    this->ifcAdded = std::make_unique<sdbusplus::bus::match::match>(
        static_cast<sdbusplus::bus::bus&>(io.get_bus()),
        sdbusplus::bus::match::rules::interfacesAdded() +
            sdbusplus::bus::match::rules::sender(
                "xyz.openbmc_project.EntityManager"),
        [this, &gpioPresence, &ctx](sdbusplus::message::message& msg) {
            ctx.spawn(interfacesAddedHandler(msg, gpioPresence));
        });

    lg2::debug("setting up dbus match for interfaces removed");

    this->ifcRemoved = std::make_unique<sdbusplus::bus::match::match>(
        static_cast<sdbusplus::bus::bus&>(io.get_bus()),
        sdbusplus::bus::match::rules::interfacesRemoved() +
            sdbusplus::bus::match::rules::sender(
                "xyz.openbmc_project.EntityManager"),
        [&gpioPresence](sdbusplus::message::message& msg) {
            sdbusplus::message::object_path objPath;
            msg.read(objPath);

            lg2::debug("detected interface removed on {OBJPATH}", "OBJPATH",
                       objPath);
            gpioPresence->removeObj(objPath);
        });
    co_return;
}

} // namespace gpio_presence
