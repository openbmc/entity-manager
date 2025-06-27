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
/// \file entity_manager.hpp

#pragma once

#include "dbus_interface.hpp"
#include "topology.hpp"
#include "utils.hpp"

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <string>

class EntityManager
{
  public:
    explicit EntityManager(
        std::shared_ptr<sdbusplus::asio::connection>& systemBus,
        boost::asio::io_context& io);

    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    sdbusplus::asio::object_server objServer;
    std::shared_ptr<sdbusplus::asio::dbus_interface> entityIface;
    nlohmann::json lastJson;
    nlohmann::json systemConfiguration;
    Topology topology;
    boost::asio::io_context& io;

    dbus_interface::EMDBusInterface dbus_interface;

    em_utils::PowerStatusMonitor powerStatus;

    void propertiesChangedCallback();
    void registerCallback(const std::string& path);
    void publishNewConfiguration(const size_t& instance, size_t count,
                                 boost::asio::steady_timer& timer,
                                 nlohmann::json newConfiguration);
    void postToDbus(const nlohmann::json& newConfiguration);
    void pruneConfiguration(bool powerOff, const std::string& name,
                            const nlohmann::json& device);

    void initFilters(const std::set<std::string>& probeInterfaces);

    void handleCurrentConfigurationJson();

  private:
    std::unique_ptr<sdbusplus::bus::match_t> nameOwnerChangedMatch = nullptr;
    std::unique_ptr<sdbusplus::bus::match_t> interfacesAddedMatch = nullptr;
    std::unique_ptr<sdbusplus::bus::match_t> interfacesRemovedMatch = nullptr;

    bool scannedPowerOff = false;
    bool scannedPowerOn = false;

    bool propertiesChangedInProgress = false;
    boost::asio::steady_timer propertiesChangedTimer;
    size_t propertiesChangedInstance = 0;

    boost::container::flat_map<std::string, sdbusplus::bus::match_t>
        dbusMatches;

    void startRemovedTimer(boost::asio::steady_timer& timer,
                           nlohmann::json& systemConfiguration);
};
