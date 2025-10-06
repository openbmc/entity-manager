#pragma once

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <string>

namespace managed_host
{

std::string hostPowerPath(size_t hostIndex);

// @returns < 0 on error
ssize_t hostIndexFromPath(const std::string& path);

// @param name     device name used for error logging
// @param config   reference to a single EM config
size_t getManagedHostIndex(const std::string& name,
                           const nlohmann::json::object_t* config);

} // namespace managed_host
