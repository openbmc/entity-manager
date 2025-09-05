#pragma once

#include "perform_scan.hpp"

#include <boost/container/flat_map.hpp>

#include <memory>
#include <string>
#include <vector>

namespace probe
{
struct CmpStr
{
    bool operator()(const char* a, const char* b) const
    {
        return std::strcmp(a, b) < 0;
    }
};

// underscore T for collison with dbus c api
enum class probe_type_codes
{
    FALSE_T,
    TRUE_T,
    AND,
    OR,
    JOINT,
    FOUND,
    MATCH_ONE
};

using FoundProbeTypeT = std::optional<probe_type_codes>;

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

// find the intersection of two sets of devices based on the object path, return
// the result in set1. set2 is destroyed.
// Note: if there are duplicates in either set or both, the duplicates will be
// kept if they are in the intersection.
inline void intersectDevices(::scan::FoundDevices& set1,
                             ::scan::FoundDevices& set2)
{
    for (auto itrDev = set2.begin(); itrDev != set2.end();)
    {
        auto findIt = std::find_if(set1.begin(), set1.end(),
                                   [&itrDev](const auto& existingDev) {
                                       return existingDev.path == itrDev->path;
                                   });
        if (findIt == set1.end())
        {
            itrDev = set2.erase(itrDev);
        }
        else
        {
            ++itrDev;
        }
    }

    for (auto itrDev = set1.begin(); itrDev != set1.end();)
    {
        auto findIt = std::find_if(set2.begin(), set2.end(),
                                   [&itrDev](const auto& existingDev) {
                                       return existingDev.path == itrDev->path;
                                   });
        if (findIt == set2.end())
        {
            itrDev = set1.erase(itrDev);
        }
        else
        {
            ++itrDev;
        }
    }
    set1.insert(set1.end(), set2.begin(), set2.end());
}

} // namespace probe
