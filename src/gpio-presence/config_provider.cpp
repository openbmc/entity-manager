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

#include "interfaces.hpp"
#include "pdi-gen/GPIODeviceDetect/client.hpp"
#include "pdi-gen/GPIODeviceDetect/common.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <string>

namespace gpio_presence
{

ConfigProvider::ConfigProvider(sdbusplus::async::context& io) : io(io) {}

void ConfigProvider::interfacesAddedHandler(
    sdbusplus::message::message& msg,
    const std::function<void(const sdbusplus::message::object_path& obj,
                             std::unique_ptr<Config> config)>&
        addConfigSingleDevice)
{
    sdbusplus::message::object_path objPath;
    SensorData ifcAndProperties;
    msg.read(objPath, ifcAndProperties);

    auto managedObjs = std::make_shared<ManagedObjectType>();

    (*managedObjs)[objPath] = ifcAndProperties;

    setupConfiguration(boost::system::error_code(), *managedObjs,
                       addConfigSingleDevice);
}

void ConfigProvider::setupConfigurationSingleDevice(
    const sdbusplus::message::object_path& obj, const SensorData& item,
    const std::function<void(const sdbusplus::message::object_path& obj,
                             std::unique_ptr<Config> config)>&
        addConfigSingleDevice)
{
    auto found = item.find(interfaces::emConfigurationGPIOHWDetectInterface);
    if (found == item.end())
    {
        return;
    }

    try
    {
        int index = obj.str.rfind('/');
        std::string name = obj.str.substr(index + 1);
        auto config = std::make_unique<Config>(io, obj, item, name);
        addConfigSingleDevice(obj, std::move(config));
    }
    catch (std::exception& e)
    {
        lg2::error("Incomplete config found: {ERR} obj = {OBJPATH}", "ERR",
                   e.what(), "OBJPATH", obj.str);
        return;
    }
}

void ConfigProvider::setupConfiguration(
    const boost::system::error_code ec, const ManagedObjectType& managedObjs,
    const std::function<void(const sdbusplus::message::object_path& obj,
                             std::unique_ptr<Config> config)>&
        addConfigSingleDevice)
{
    if (ec)
    {
        lg2::error("error calling 'GetManagedObjects': {MSG}", "MSG",
                   ec.message());
        return;
    }
    for (const auto& obj : managedObjs)
    {
        sdbusplus::message::object_path path = obj.first;
        const SensorData& second = obj.second;
        setupConfigurationSingleDevice(path, second, addConfigSingleDevice);
    }
}

// NOLINTBEGIN
sdbusplus::async::task<> ConfigProvider::initialize(
    sdbusplus::async::context& ctx,
    const std::function<void(const sdbusplus::message::object_path& path)>&
        removeHandler,
    const std::function<void(const sdbusplus::message::object_path& obj,
                             std::unique_ptr<Config> config)>&
        addConfigSingleDevice)
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

            sdbusplus::message::object_path obj = path;
            auto config = std::make_unique<Config>(ctx, obj, presenceGpioNames,
                                                   presenceGpioValues, name);
            addConfigSingleDevice(path, std::move(config));
        }
    }

    lg2::debug("setting up dbus match for interfaces added...");

    this->ifcAdded = std::make_unique<sdbusplus::bus::match::match>(
        static_cast<sdbusplus::bus::bus&>(io.get_bus()),
        sdbusplus::bus::match::rules::interfacesAdded() +
            sdbusplus::bus::match::rules::sender(
                "xyz.openbmc_project.EntityManager"),
        [this, addConfigSingleDevice](sdbusplus::message::message& msg) {
            interfacesAddedHandler(msg, addConfigSingleDevice);
        });

    lg2::debug("done setting up dbus match for interfacess added");

    this->ifcRemoved = std::make_unique<sdbusplus::bus::match::match>(
        static_cast<sdbusplus::bus::bus&>(io.get_bus()),
        sdbusplus::bus::match::rules::interfacesRemoved() +
            sdbusplus::bus::match::rules::sender(
                "xyz.openbmc_project.EntityManager"),
        [removeHandler](sdbusplus::message::message& msg) {
            sdbusplus::message::object_path objPath;
            msg.read(objPath);

            lg2::debug("detected interface removed on {OBJPATH}", "OBJPATH",
                       objPath);
            removeHandler(objPath);
        });
    co_return;
}

} // namespace gpio_presence
