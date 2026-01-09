#pragma once

#include <nlohmann/json.hpp>

#include <flat_map>
#include <optional>
#include <string>
#include <vector>

class EMConfig
{
  public:
    explicit EMConfig(const nlohmann::json::object_t& config);
    static std::optional<EMConfig> fromJson(const nlohmann::json& config);

    nlohmann::json toJson();

    std::string name;
    std::string type;
    std::vector<std::string> probeStmt;

    // "Logging" property
    bool deviceHasLogging;

    std::vector<nlohmann::json::object_t> exposesRecords;

    // map of extra interfaces starting with 'xyz.openbmc_project....'
    std::flat_map<std::string, nlohmann::json::object_t> extraInterfaces;
};
