#pragma once

#include "perform_scan.hpp"

#include <flat_map>
#include <memory>
#include <string>
#include <vector>

namespace probe
{

// this class finds the needed dbus fields and on destruction runs the probe
struct PerformProbe
{
    PerformProbe(nlohmann::json& recordRef,
                 const std::vector<std::string>& probeCommand,
                 std::string probeName,
                 std::shared_ptr<scan::PerformScan>& scanPtr);
    virtual ~PerformProbe();

  private:
    nlohmann::json& recordRef;
    std::vector<std::string> _probeCommand;
    std::string probeName;
    std::shared_ptr<scan::PerformScan> scan;
};

} // namespace probe
