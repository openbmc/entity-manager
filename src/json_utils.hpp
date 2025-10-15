#pragma once

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <string>
#include <string_view>

const std::string* getStringFromObject(const nlohmann::json::object_t& object,
                                       std::string_view key);

std::string* getStringFromObject(nlohmann::json::object_t& object,
                                 std::string_view key);
