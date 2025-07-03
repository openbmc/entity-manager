#pragma once

#include "../utils.hpp"
#include "dbus_probe.hpp"
#include "entity_manager.hpp"

#include <systemd/sd-journal.h>

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

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
                std::vector<DbusProbe>& probes,
                std::function<void()>&& callback);

    void updateSystemConfiguration(const nlohmann::json& recordRef,
                                   const std::string& probeName,
                                   FoundDevices& foundDevices);
    void run();
    virtual ~PerformScan();
    EntityManager& _em;
    nlohmann::json& _missingConfigurations;
    std::vector<nlohmann::json> _configurations;
    std::vector<DbusProbe> _probes;
    std::function<void()> _callback;
    bool _passed = false;
    MapperGetSubTreeResponse dbusProbeObjects;
    std::vector<std::string> passedProbes;
};

} // namespace scan
