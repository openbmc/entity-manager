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

#include <ranges>
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
    using Mapper = sdbusplus::common::xyz::openbmc_project::ObjectMapper;

    auto client = sdbusplus::client::xyz::openbmc_project::ObjectMapper<>(ctx)
                      .service(Mapper::default_service)
                      .path(Mapper::instance_path);

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
              "ERR", e);
    }

    if (res.empty())
    {
        co_return;
    }

    // call the user callback for all the device that is already available
    for (auto& [path, serviceInterfaceMap] : res)
    {
        for (const auto& service :
             std::ranges::views::keys(serviceInterfaceMap))
        {
            debug("found configuration interface at {SERVICE} {PATH} {INTF}",
                  "SERVICE", service, "PATH", path, "INTF", interface);

            addConfig(path);
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
        ctx, rules_intf::interfacesAdded() + senderRule);

    while (!ctx.stop_requested())
    {
        auto tmp = co_await addedMatch
                       .next<sdbusplus::message::object_path, ConfigData>();

        auto [objPath, intfMap] = std::move(tmp);

        debug("Detected interface added on {OBJPATH}", "OBJPATH", objPath);

        if (!std::ranges::contains(std::views::keys(intfMap), interface))
        {
            continue;
        }

        try
        {
            addConfig(objPath);
        }
        catch (std::exception& e)
        {
            error("Incomplete or invalid config found: {ERR}", "ERR", e);
        }
    }
};

auto ConfigProvider::handleInterfacesRemoved(RemovedCallback removeConfig)
    -> sdbusplus::async::task<void>
{
    debug("setting up dbus match for interfaces removed");

    sdbusplus::async::match removedMatch(
        ctx, rules_intf::interfacesRemoved() + senderRule);

    while (!ctx.stop_requested())
    {
        auto tmp = co_await removedMatch.next<sdbusplus::message::object_path,
                                              std::vector<std::string>>();
        auto [objectPath, interfaces] = std::move(tmp);

        if (!std::ranges::contains(interfaces, interface))
        {
            continue;
        }

        debug("Detected interface {INTF} removed on {OBJPATH}", "INTF",
              interface, "OBJPATH", objectPath);

        removeConfig(objectPath);
    }
};

} // namespace gpio_presence
