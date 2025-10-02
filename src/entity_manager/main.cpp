// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "entity_manager.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

int main()
{
    const std::vector<std::filesystem::path> configurationDirectories = {
        PACKAGE_DIR "configurations", SYSCONF_DIR "configurations"};

    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    systemBus->request_name("xyz.openbmc_project.EntityManager");
    EntityManager em(systemBus, io, configurationDirectories);

    nlohmann::json systemConfiguration = nlohmann::json::object();

    boost::asio::post(io, [&]() { em.propertiesChangedCallback(); });

    em.handleCurrentConfigurationJson();

    io.run();

    return 0;
}
