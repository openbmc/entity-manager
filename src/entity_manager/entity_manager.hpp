// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once

#include "configuration.hpp"
#include "dbus_interface.hpp"
#include "power_status_monitor.hpp"
#include "topology.hpp"

#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <flat_map>
#include <string>

class EntityManager
{
  public:
    explicit EntityManager(
        std::shared_ptr<sdbusplus::asio::connection>& systemBus,
        boost::asio::io_context& io,
        const std::vector<std::filesystem::path>& configurationDirectories,
        const std::filesystem::path& schemaDirectory);

    // disable copy
    EntityManager(const EntityManager&) = delete;
    EntityManager& operator=(const EntityManager&) = delete;

    // disable move
    EntityManager(EntityManager&&) = delete;
    EntityManager& operator=(EntityManager&&) = delete;

    ~EntityManager() = default;

    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    sdbusplus::asio::object_server objServer;
    std::shared_ptr<sdbusplus::asio::dbus_interface> entityIface;
    Configuration configuration;
    nlohmann::json lastJson;
    nlohmann::json systemConfiguration;
    Topology topology;
    boost::asio::io_context& io;

    dbus_interface::EMDBusInterface dbus_interface;

    power::PowerStatusMonitor powerStatus;

    void propertiesChangedCallback();
    void registerCallback(const std::string& path);
    void publishNewConfiguration(const size_t& instance, size_t count,
                                 boost::asio::steady_timer& timer,
                                 nlohmann::json newConfiguration);
    void postToDbus(const nlohmann::json& newConfiguration);
    void postBoardToDBus(const std::string& boardId,
                         const nlohmann::json::object_t& boardConfig,
                         std::map<std::string, std::string>& newBoards);
    void postExposesRecordsToDBus(
        nlohmann::json& item, size_t& exposesIndex,
        const std::string& boardNameOrig, std::string jsonPointerPath,
        const std::string& jsonPointerPathBoard, const std::string& boardPath,
        const std::string& boardType);

    // @returns false on error
    bool postConfigurationRecord(
        const std::string& name, nlohmann::json& config,
        const std::string& boardNameOrig, const std::string& itemType,
        const std::string& jsonPointerPath, const std::string& ifacePath);

    void pruneConfiguration(bool powerOff, const std::string& name,
                            const nlohmann::json& device);

    void handleCurrentConfigurationJson();

    bool isPropertiesChangedInProgress() const;
    size_t getPropertiesChangedInstance() const;
    bool hasDBusMatch(const sdbusplus::message::object_path& path) const;

  private:
    std::unique_ptr<sdbusplus::bus::match_t> nameOwnerChangedMatch = nullptr;
    std::unique_ptr<sdbusplus::bus::match_t> interfacesAddedMatch = nullptr;
    std::unique_ptr<sdbusplus::bus::match_t> interfacesRemovedMatch = nullptr;

    bool scannedPowerOff = false;
    bool scannedPowerOn = false;

    bool propertiesChangedInProgress = false;
    boost::asio::steady_timer propertiesChangedTimer;
    size_t propertiesChangedInstance = 0;

    std::flat_map<std::string, sdbusplus::bus::match_t, std::less<>>
        dbusMatches;

    void startRemovedTimer(boost::asio::steady_timer& timer,
                           nlohmann::json& systemConfiguration);
    void initFilters(const std::unordered_set<std::string>& probeInterfaces);
};
