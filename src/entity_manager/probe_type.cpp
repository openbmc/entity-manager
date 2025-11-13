#include "probe_type.hpp"

#include <flat_map>
#include <string>
#include <string_view>

namespace probe
{

FoundProbeTypeT findProbeType(const std::string& probe)
{
    static const std::flat_map<std::string_view, probe_type_codes, std::less<>>
        probeTypes{{{"FALSE", probe_type_codes::FALSE_T},
                    {"TRUE", probe_type_codes::TRUE_T},
                    {"AND", probe_type_codes::AND},
                    {"OR", probe_type_codes::OR},
                    {"FOUND", probe_type_codes::FOUND},
                    {"MATCH_ONE", probe_type_codes::MATCH_ONE}}};

    std::flat_map<std::string_view, probe_type_codes,
                  std::less<>>::const_iterator probeType;
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
