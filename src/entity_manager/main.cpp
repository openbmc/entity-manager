/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "configuration.hpp"
#include "entity_manager.hpp"
#include "utils.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

int main()
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    systemBus->request_name("xyz.openbmc_project.EntityManager");
    EntityManager em(systemBus, io);

    nlohmann::json systemConfiguration = nlohmann::json::object();

    std::set<std::string> probeInterfaces = configuration::getProbeInterfaces();

    em.initFilters(probeInterfaces);

    boost::asio::post(io, [&]() { em.propertiesChangedCallback(); });

    em.handleCurrentConfigurationJson();

    // some boards only show up after power is on, we want to not say they are
    // removed until the same state happens
    em.emUtils.setupPowerMatch(em.systemBus);

    io.run();

    return 0;
}
