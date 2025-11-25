#pragma once

#include "../utils.hpp"
#include "entity_manager.hpp"

#include <systemd/sd-journal.h>

#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <flat_map>
#include <functional>
#include <list>
#include <vector>

namespace scan
{
struct DBusDeviceDescriptor
{
    DBusInterface interface;
    std::string path;
};

using FoundDevices = std::vector<DBusDeviceDescriptor>;

struct PerformScan : std::enable_shared_from_this<PerformScan>
{
    PerformScan(EntityManager& em, nlohmann::json& missingConfigurations,
                std::vector<nlohmann::json>& configurations,
                boost::asio::io_context& io,
                std::chrono::milliseconds getInterfacesTimeoutMillis,
                std::chrono::milliseconds findDBusObjectsTimeoutMillis,
                std::function<void()>&& callback);

    void updateSystemConfiguration(const nlohmann::json& recordRef,
                                   const std::string& probeName,
                                   FoundDevices& foundDevices);
    void run();
    virtual ~PerformScan();
    EntityManager& _em;
    MapperGetSubTreeResponse dbusProbeObjects;
    std::vector<std::string> passedProbes;

  private:
    nlohmann::json& _missingConfigurations;
    std::vector<nlohmann::json> _configurations;
    std::function<void()> _callback;
    bool _passed = false;

    boost::asio::io_context& io;

    const std::chrono::milliseconds getInterfacesTimeoutMillis;
    const std::chrono::milliseconds findDBusObjectsTimeoutMillis;
};

} // namespace scan
