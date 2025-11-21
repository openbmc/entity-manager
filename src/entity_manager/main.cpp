// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "entity_manager.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <chrono>

int main()
{
    const std::vector<std::filesystem::path> configurationDirectories = {
        PACKAGE_DIR "configurations", SYSCONF_DIR "configurations"};

    const std::filesystem::path schemaDirectory(PACKAGE_DIR "schemas");

    const std::filesystem::path configurationOutDir("/var/configuration");

    const std::chrono::milliseconds propertiesChangedTimeoutMilliseconds(500);

    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    systemBus->request_name("xyz.openbmc_project.EntityManager");
    EntityManager em(systemBus, io, configurationDirectories, schemaDirectory,
                     true, configurationOutDir,
                     propertiesChangedTimeoutMilliseconds);

    boost::asio::post(io, [&]() { em.propertiesChangedCallback(); });

    em.handleCurrentConfigurationJson();

    io.run();

    return 0;
}
