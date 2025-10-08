// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#pragma once

#include <sdbusplus/asio/connection.hpp>

#include <set>

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

namespace object_mapper
{

void getSubTree(
    std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    const std::string& path, int32_t depth,
    const std::set<std::string>& interfaces,
    std::function<void(boost::system::error_code& ec,
                       const GetSubTreeType& interfaceSubtree)>& callback);

};
