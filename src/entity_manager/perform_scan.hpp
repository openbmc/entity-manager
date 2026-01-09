#pragma once

#include "../utils.hpp"
#include "em_config.hpp"
#include "entity_manager.hpp"
#include "probe_lexer.hpp"

#include <systemd/sd-journal.h>

#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <flat_map>
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
    PerformScan(EntityManager& em, nlohmann::json& missingConfigurations,
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

    nlohmann::json& _missingConfigurations;
    std::vector<EMConfig> _configurations;
    std::function<void()> _callback;
    bool _passed = false;

    boost::asio::io_context& io;
};

namespace detail
{
// Resolve configuration templates and apply expose actions. The configuration
// is published temporarily for same-configuration references; callers must
// publish the final record after applying actions.
void applyTemplatesAndExposeActions(
    const std::string& recordName, EMConfig& record,
    const DBusObject& dbusObject, size_t foundDeviceIdx,
    std::optional<std::string>& replaceStr,
    nlohmann::json& systemConfiguration);

// Parse validated probe statements into a token stream. The statements are
// joined with single spaces and lexed. Returns an empty vector on a lexing
// error; a valid probe is never empty.
std::vector<probe::Token> parseProbeCommand(
    const std::vector<std::string>& probeField);
std::string getRecordName(const DBusInterface& probe,
                          const std::string& probeName);
void restorePersistedConfigurations(
    FoundDevices& foundDevices, const std::string& probeName,
    nlohmann::json& systemConfiguration, nlohmann::json& lastJson,
    nlohmann::json& missingConfigurations,
    std::vector<std::string>& passedProbes, std::set<nlohmann::json>& usedNames,
    std::list<size_t>& indexes);
} // namespace detail

} // namespace scan
