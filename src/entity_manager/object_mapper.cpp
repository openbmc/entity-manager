// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#include "object_mapper.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <set>

void object_mapper::getSubTree(
    std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    const std::string& path, const int32_t depth,
    const std::set<std::string>& interfaces,
    std::function<void(boost::system::error_code& ec,
                       const GetSubTreeType& interfaceSubtree)>& callback)
{
    lg2::debug("GetSubTree path={PATH} depth={DEPTH} of {N} interfaces", "PATH",
               path, "DEPTH", depth, "N", interfaces.size());

    systemBus->async_method_call(callback, "xyz.openbmc_project.ObjectMapper",
                                 "/xyz/openbmc_project/object_mapper",
                                 "xyz.openbmc_project.ObjectMapper",
                                 "GetSubTree", path, depth, interfaces);
}
