#pragma once

#include "em_config.hpp"

#include <nlohmann/json.hpp>

#include <flat_map>
#include <optional>
#include <string>

// This is mostly just a map of hashes to json objects.
// There are some cases where a string might be used as key,
// that's why it's a string and not integer type.
using SystemConfiguration = std::flat_map<std::string, EMConfig>;

std::optional<SystemConfiguration> systemConfigurationFromJson(
    const nlohmann::json& json);
