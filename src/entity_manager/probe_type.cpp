#include "probe_type.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace probe
{

FoundProbeTypeT findProbeType(const std::string& probe)
{
    // The boolean/logical probe-type keywords are complete, standalone tokens
    // in a "Probe" statement list (e.g. "AND", "OR", "TRUE"). Compare them
    // exactly so that a D-Bus probe whose contents merely contain a keyword as
    // a substring (e.g. a FRU product name "HORIZON", which contains "OR") is
    // not misclassified as a probe-type command.
    static constexpr std::array<std::pair<std::string_view, probe_type_codes>,
                                5>
        keywords{{{"FALSE", probe_type_codes::FALSE_T},
                  {"TRUE", probe_type_codes::TRUE_T},
                  {"AND", probe_type_codes::AND},
                  {"OR", probe_type_codes::OR},
                  {"MATCH_ONE", probe_type_codes::MATCH_ONE}}};

    // Trim surrounding whitespace to tolerate incidental spacing in JSON.
    const auto begin = probe.find_first_not_of(" \t");
    if (begin == std::string::npos)
    {
        return std::nullopt;
    }
    const auto end = probe.find_last_not_of(" \t");
    const std::string_view token(&probe[begin], end - begin + 1);

    // FOUND is a function-style command: FOUND('<name>').
    if (token.starts_with("FOUND("))
    {
        return probe_type_codes::FOUND;
    }

    for (const auto& [keyword, code] : keywords)
    {
        if (token == keyword)
        {
            return code;
        }
    }

    return std::nullopt;
}

} // namespace probe
