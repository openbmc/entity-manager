#pragma once

#include <optional>
#include <string>

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

} // namespace probe
