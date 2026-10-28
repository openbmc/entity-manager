#pragma once

#include "em_config.hpp"

#include <nlohmann/json.hpp>

#include <flat_map>
#include <optional>
#include <string>

// Map board identifiers to configurations with template variables resolved.
using SystemConfiguration = std::flat_map<std::string, EMConfig>;

// Parse persisted configurations, removing legacy null Exposes slots.
std::optional<SystemConfiguration> systemConfigurationFromJson(
    const nlohmann::json& json);
