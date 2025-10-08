// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "object_mapper.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <xyz/openbmc_project/ObjectMapper/common.hpp>

#include <flat_set>

void object_mapper::getSubTree(
    sdbusplus::asio::connection& systemBus, const std::string& path,
    const int32_t depth, const std::flat_set<std::string>& interfaces,
    std::move_only_function<void(boost::system::error_code& ec,
                                 const GetSubTreeType& interfaceSubtree)>&&
        callback)
{
    using ObjectMapper = sdbusplus::common::xyz::openbmc_project::ObjectMapper;

    lg2::debug("GetSubTree path={PATH} depth={DEPTH} of {N} interfaces", "PATH",
               path, "DEPTH", depth, "N", interfaces.size());

    systemBus.async_method_call(
        std::move(callback), ObjectMapper::default_service,
        ObjectMapper::instance_path, ObjectMapper::interface,
        ObjectMapper::method_names::get_sub_tree, path, depth, interfaces);
}
