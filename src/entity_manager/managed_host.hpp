#pragma once

#include <nlohmann/json.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <string>

namespace managed_host
{

sdbusplus::object_path hostPowerPath(size_t hostIndex);

// @returns < 0 on error
ssize_t hostIndexFromPath(const sdbusplus::object_path& path);

// @param name     device name used for error logging
// @param config   reference to a single EM config
size_t getManagedHostIndex(const std::string& name,
                           const nlohmann::json::object_t* config);

} // namespace managed_host
