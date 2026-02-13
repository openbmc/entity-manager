#pragma once

#include "../utils.hpp"
#include "entity_manager.hpp"

#include <systemd/sd-journal.h>

#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <flat_map>
#include <functional>
#include <list>
#include <set>
#include <vector>

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
    PerformScan(EntityManager& em, nlohmann::json& missingConfigurations,
                std::vector<nlohmann::json>& configurations,
                boost::asio::io_context& io, std::function<void()>&& callback);

    void updateSystemConfiguration(const nlohmann::json& recordRef,
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

    nlohmann::json& _missingConfigurations;
    std::vector<nlohmann::json> _configurations;
    std::function<void()> _callback;
    bool _passed = false;

    boost::asio::io_context& io;
};

namespace detail
{
// Parse a config "Probe" field (an array of statements, or a single statement
// string) into a list of probe statements. Returns an empty vector on error (a
// non-string statement); a valid probe is never empty.
std::vector<std::string> parseProbeCommand(const nlohmann::json& probeField);

// Collect the "Name" of every entry in systemConfiguration, i.e. the names of
// the already-applied configs.
std::vector<std::string> collectConfiguredNames(
    const nlohmann::json& systemConfiguration);

void pruneMissingByName(nlohmann::json& missingConfigurations,
                        const std::vector<std::string>& names);
} // namespace detail

} // namespace scan
