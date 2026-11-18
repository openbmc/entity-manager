#pragma once

#include "perform_scan.hpp"
#include "probe_parser.hpp"

#include <flat_map>
#include <memory>
#include <string>
#include <vector>

namespace probe
{

// this class finds the needed dbus fields and on destruction runs the probe
struct PerformProbe final
{
    PerformProbe(EMConfig& recordRef, std::string probeName,
                 std::shared_ptr<scan::PerformScan>& scanPtr);
    ~PerformProbe();

  private:
    EMConfig& recordRef;
    std::string probeName;
    std::shared_ptr<scan::PerformScan> scan;
};

namespace detail
{
// evaluate a parsed probe against the given D-Bus objects and the names of
// passed probes. Matching devices are appended to foundDevs.
bool doProbe(const Ast& ast, const MapperGetSubTreeResponse& dbusProbeObjects,
             const std::vector<std::string>& passedProbes,
             scan::FoundDevices& foundDevs);
} // namespace detail

} // namespace probe
