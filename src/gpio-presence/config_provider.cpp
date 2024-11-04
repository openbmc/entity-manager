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

using VariantType =
    std::variant<std::vector<std::string>, std::string, int64_t, uint64_t,
                 double, int32_t, uint32_t, int16_t, uint16_t, uint8_t, bool>;
using ConfigMap = boost::container::flat_map<std::string, VariantType>;
using ConfigData = boost::container::flat_map<std::string, ConfigMap>;

namespace gpio_presence
{

ConfigProvider::ConfigProvider(sdbusplus::async::context& ctx,
                               const std::string& interface) :
    interface(interface), ctx(ctx)
{}

auto ConfigProvider::initialize(AddedCallback addConfig,
                                RemovedCallback removeConfig)
    -> sdbusplus::async::task<void>
{
    ctx.spawn(handleInterfacesAdded(addConfig));
    ctx.spawn(handleInterfacesRemoved(removeConfig));

    co_await getConfig(addConfig);
}

auto ConfigProvider::getConfig(AddedCallback addConfig)
    -> sdbusplus::async::task<void>
{
    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(ctx)
                      .service("xyz.openbmc_project.ObjectMapper")
                      .path("/xyz/openbmc_project/object_mapper");

    debug("calling 'GetSubTree' to find instances of {INTF}", "INTF",
          interface);

    using SubTreeType =
        std::map<std::string, std::map<std::string, std::vector<std::string>>>;
    SubTreeType res = {};

    try
    {
        std::vector<std::string> interfaces = {interface};
        res = co_await client.get_sub_tree("/xyz/openbmc_project/inventory", 0,
                                           interfaces);
    }
    catch (std::exception& e)
    {
        error("Failed GetSubTree call for configuration interface: {ERR}",
              "ERR", e.what());
    }

    if (res.empty())
    {
        co_return;
    }

    // call the user callback for all the device that is already available
    for (auto& [path, serviceInterfaceMap] : res)
    {
        for (const auto& [service, _] : serviceInterfaceMap)
        {
            debug("found configuration interface at {SERVICE} {PATH} {INTF}",
                  "SERVICE", service, "PATH", path, "INTF", interface);

            addConfig(service, path);
        }
    }
}

namespace rules_intf = sdbusplus::bus::match::rules;

const auto senderRule = rules_intf::sender("xyz.openbmc_project.EntityManager");

auto ConfigProvider::handleInterfacesAdded(AddedCallback addConfig)
    -> sdbusplus::async::task<void>
{
    debug("setting up dbus match for interfaces added");

    sdbusplus::async::match addedMatch(
        ctx, rules_intf::interfacesAdded() + senderRule +
                 rules_intf::interface(interface));

    while (!ctx.stop_requested())
    {
        auto [objPath, serviceIntfMap] =
            co_await addedMatch
                .next<sdbusplus::message::object_path, ConfigData>();

        debug("Detected interface added on {OBJPATH}", "OBJPATH", objPath);

        try
        {
            if (serviceIntfMap.empty())
            {
                co_return;
            }

            addConfig(serviceIntfMap.begin()->first, objPath);
        }
        catch (std::exception& e)
        {
            error("Incomplete config found: {ERR}", "ERR", e);
        }
    }
};

auto ConfigProvider::handleInterfacesRemoved(RemovedCallback removeConfig)
    -> sdbusplus::async::task<void>
{
    debug("setting up dbus match for interfaces removed");

    sdbusplus::async::match removedMatch(
        ctx, rules_intf::interfacesRemoved() + senderRule +
                 rules_intf::interface(interface));

    while (!ctx.stop_requested())
    {
        auto [objectPath, msg] =
            co_await removedMatch.next<sdbusplus::message::object_path,
                                       std::vector<std::string>>();

        debug("Detected interface removed on {OBJPATH}", "OBJPATH", objectPath);

        removeConfig(objectPath);
    }
};

} // namespace gpio_presence
