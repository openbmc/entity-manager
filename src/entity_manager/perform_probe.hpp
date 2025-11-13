#pragma once

#include "perform_scan.hpp"

#include <flat_map>
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

using FoundProbeTypeT = std::optional<probe_type_codes>;

FoundProbeTypeT findProbeType(const std::string& probe);

// this class finds the needed dbus fields and on destruction runs the probe
struct PerformProbe final
{
    PerformProbe(nlohmann::json& recordRef,
                 const std::vector<std::string>& probeCommand,
                 std::string probeName,
                 std::shared_ptr<scan::PerformScan>& scanPtr);
    ~PerformProbe();

  private:
    nlohmann::json& recordRef;
    std::vector<std::string> _probeCommand;
    std::string probeName;
    std::shared_ptr<scan::PerformScan> scan;
};

} // namespace probe
