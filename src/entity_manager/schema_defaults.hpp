// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <map>
#include <string>

// Defaults are publication overlays, not changes to the stored configuration.
class SchemaDefaults
{
  public:
    explicit SchemaDefaults(const std::filesystem::path& schemaDirectory);

    nlohmann::json exposes(const nlohmann::json& record) const;

  private:
    void addType(const nlohmann::json& type, const nlohmann::json& defaults);

    std::map<std::string, nlohmann::json, std::less<>> exposesDefaults;
};
