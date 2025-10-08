// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright OpenBMC Authors

#pragma once

#include <sdbusplus/asio/connection.hpp>

#include <flat_set>

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

namespace object_mapper
{

void getSubTree(
    sdbusplus::asio::connection& systemBus, const std::string& path,
    int32_t depth, const std::flat_set<std::string>& interfaces,
    std::move_only_function<void(boost::system::error_code& ec,
                                 const GetSubTreeType& interfaceSubtree)>&&
        callback);

};
