#pragma once

#include "perform_scan.hpp"

#include <boost/container/flat_map.hpp>

#include <memory>
#include <string>
#include <vector>

namespace probe
{
// underscore T for collison with dbus c api
enum class probe_type_codes
{
    FALSE_T,
    TRUE_T,
    AND,
    OR,
    FOUND,
    MATCH_ONE
};

using FoundProbeTypeT = std::optional<boost::container::flat_map<
    std::string, probe_type_codes>::const_iterator>;

FoundProbeTypeT findProbeType(const std::string& probe);

// this class finds the needed dbus fields and on destruction runs the probe
struct PerformProbe : std::enable_shared_from_this<PerformProbe>
{
    PerformProbe(nlohmann::json& recordRef,
                 const std::vector<std::string>& probeCommand,
                 std::string probeName,
                 std::shared_ptr<scan::PerformScan>& scanPtr);
    virtual ~PerformProbe();

    nlohmann::json& recordRef;
    std::vector<std::string> _probeCommand;
    std::string probeName;
    std::shared_ptr<scan::PerformScan> scan;
};

} // namespace probe
