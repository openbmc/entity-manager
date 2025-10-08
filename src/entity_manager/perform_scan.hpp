#pragma once

#include "../utils.hpp"
#include "entity_manager.hpp"
#include "object_mapper.hpp"

#include <systemd/sd-journal.h>

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <flat_set>
#include <functional>
#include <vector>

namespace probe
{
struct PerformProbe;
};

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
                boost::asio::io_context& io, std::function<void()>&& callback);

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
};

void findDbusObjects(
    std::vector<std::shared_ptr<probe::PerformProbe>> probeVector,
    std::flat_set<std::string> interfaces,
    const std::shared_ptr<scan::PerformScan>& scan, boost::asio::io_context& io,
    size_t retries = 5);

void afterFindDBusObjects(
    boost::asio::io_context& io, std::flat_set<std::string> interfaces,
    std::vector<std::shared_ptr<probe::PerformProbe>> probeVector,
    const std::shared_ptr<scan::PerformScan>& scan, size_t retries,
    boost::system::error_code ec, const GetSubTreeType& interfaceSubtree);

} // namespace scan
