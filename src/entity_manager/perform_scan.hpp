#pragma once

#include "../utils.hpp"
#include "em_config.hpp"
#include "entity_manager.hpp"
#include "system_configuration.hpp"

#include <systemd/sd-journal.h>

#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <flat_set>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <vector>

namespace probe
{
struct PerformProbe;
}

namespace scan
{
struct DBusDeviceDescriptor
{
    DBusInterface interface;
    std::string path;
};

using FoundDevices = std::vector<DBusDeviceDescriptor>;

struct PerformScan final : std::enable_shared_from_this<PerformScan>
{
    PerformScan(EntityManager& em, SystemConfiguration& missingConfigurations,
                std::vector<EMConfig>& configurations,
                boost::asio::io_context& io, std::function<void()>&& callback);

    void updateSystemConfiguration(const EMConfig& recordRef,
                                   const std::string& probeName,
                                   FoundDevices& foundDevices);
    void run();
    ~PerformScan();
    EntityManager& _em;
    MapperGetSubTreeResponse dbusProbeObjects;
    std::vector<std::string> passedProbes;

  private:
    void restorePersistedConfigurations(
        FoundDevices& foundDevices, const std::string& probeName,
        std::set<nlohmann::json>& usedNames, std::list<size_t>& indexes);

    void updateSystemConfigurationForDevice(
        const EMConfig& recordRef, const std::string& probeName,
        const DBusDeviceDescriptor& device, std::set<nlohmann::json>& usedNames,
        std::list<size_t>& indexes, std::optional<std::string>& replaceStr);

    // Walk _configurations, dropping malformed or already-probed entries and
    // starting a PerformProbe for each remaining one. Collects the D-Bus
    // interfaces to look up into dbusProbeInterfaces / dbusProbePointers.
    // Returns false if a config had an unparsable Probe, in which case the
    // scan must not continue.
    bool processConfigurations(
        std::flat_set<std::string, std::less<>>& dbusProbeInterfaces,
        std::vector<std::shared_ptr<probe::PerformProbe>>& dbusProbePointers);

    SystemConfiguration& _missingConfigurations;
    std::vector<EMConfig> _configurations;
    std::function<void()> _callback;
    bool _passed = false;

    boost::asio::io_context& io;
};

} // namespace scan
