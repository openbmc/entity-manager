// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "perform_probe.hpp"

#include "entity_manager.hpp"
#include "perform_scan.hpp"
#include "system_mapper.hpp"

#include <phosphor-logging/lg2.hpp>

#include <utility>

namespace probe
{

PerformProbe::PerformProbe(nlohmann::json& recordRef,
                           const std::vector<std::string>& probeCommand,
                           std::string probeName,
                           std::shared_ptr<scan::PerformScan>& scanPtr) :
    recordRef(recordRef), _probeCommand(probeCommand),
    probeName(std::move(probeName)), scan(scanPtr)
{}

PerformProbe::~PerformProbe()
{
    scan::FoundDevices foundDevs;
    if (scan->_em.systemMapper.doProbe(_probeCommand, scan, foundDevs))
    {
        scan->updateSystemConfiguration(recordRef, probeName, foundDevs);
    }
}

FoundProbeTypeT findProbeType(const std::string& probe)
{
    static const boost::container::flat_map<const char*, probe_type_codes,
                                            CmpStr>
        probeTypes{{{"FALSE", probe_type_codes::FALSE_T},
                    {"TRUE", probe_type_codes::TRUE_T},
                    {"AND", probe_type_codes::AND},
                    {"OR", probe_type_codes::OR},
                    {"FOUND", probe_type_codes::FOUND},
                    {"MATCH_ONE", probe_type_codes::MATCH_ONE}}};

    boost::container::flat_map<const char*, probe_type_codes,
                               CmpStr>::const_iterator probeType;
    for (probeType = probeTypes.begin(); probeType != probeTypes.end();
         ++probeType)
    {
        if (probe.find(probeType->first) != std::string::npos)
        {
            return probeType->second;
        }
    }

    return std::nullopt;
}

} // namespace probe
