/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
 */
#include "config_provider.hpp"

#include <boost/container/flat_map.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/match.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>

#include <string>

PHOSPHOR_LOG2_USING;

using BasicVariantType =
    std::variant<std::vector<std::string>, std::string, int64_t, uint64_t,
                 double, int32_t, uint32_t, int16_t, uint16_t, uint8_t, bool>;
using SensorBaseConfigMap =
    boost::container::flat_map<std::string, BasicVariantType>;
using SensorData = boost::container::flat_map<std::string, SensorBaseConfigMap>;

namespace gpio_presence
{

ConfigProvider::ConfigProvider(sdbusplus::async::context& ctx,
                               const std::string& interface) :
    dbusInterface(interface), ctx(ctx)
{}

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<> ConfigProvider::initialize(
    ConfigAddCallback addConfig, ConfigRemoveCallback removeConfig)
// NOLINTEND(readability-static-accessed-through-instance)
{
    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(ctx)
                      .service("xyz.openbmc_project.ObjectMapper")
                      .path("/xyz/openbmc_project/object_mapper");

    debug("calling 'GetSubTree' to find instances of {INTF}", "INTF",
          dbusInterface);

    using SubTreeType =
        std::map<std::string, std::map<std::string, std::vector<std::string>>>;
    SubTreeType res = {};

    try
    {
        res = co_await client.get_sub_tree("/xyz/openbmc_project/inventory", 0,
                                           {dbusInterface});
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
                  "SERVICE", service, "PATH", path, "INTF", dbusInterface);

            addConfig(service, path);
        }
    }

    debug("setting up dbus match for interfaces added");

    ctx.spawn(handleInterfacesAdded(addConfig));

    debug("setting up dbus match for interfaces removed");

    ctx.spawn(handleInterfacesRemoved(removeConfig));

    debug("Initialization completed");
}

const auto matchRuleSenderEM =
    sdbusplus::bus::match::rules::sender("xyz.openbmc_project.EntityManager");

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> ConfigProvider::handleInterfacesAdded(
    ConfigAddCallback addConfig)
// NOLINTEND(readability-static-accessed-through-instance)
{
    const auto matchRuleWhichInterface =
        sdbusplus::bus::match::rules::interface(dbusInterface);

    sdbusplus::async::match ifcAddedMatch(
        ctx, sdbusplus::bus::match::rules::interfacesAdded() +
                 matchRuleSenderEM + matchRuleWhichInterface);

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

            addConfig(service, objPath);
        }
        catch (std::exception& e)
        {
            error("Incomplete config found: {ERR}", "ERR", e.what());
        }
    }
};

// NOLINTBEGIN(readability-static-accessed-through-instance)
sdbusplus::async::task<void> ConfigProvider::handleInterfacesRemoved(
    ConfigRemoveCallback removeConfig)
// NOLINTEND(readability-static-accessed-through-instance)
{
    const auto matchRuleWhichInterface =
        sdbusplus::bus::match::rules::interface(dbusInterface);

    sdbusplus::async::match ifcRemovedMatch(
        ctx, sdbusplus::bus::match::rules::interfacesRemoved() +
                 matchRuleSenderEM + matchRuleWhichInterface);

    while (!ctx.stop_requested())
    {
        auto [objectPath, msg] =
            co_await ifcRemovedMatch.next<sdbusplus::message::object_path,
                                          std::vector<std::string>>();

        debug("Called InterfacesRemoved handler");

        debug("Detected interface removed on {OBJPATH}", "OBJPATH", objectPath);

        removeConfig(objectPath);
    }
};

} // namespace gpio_presence
