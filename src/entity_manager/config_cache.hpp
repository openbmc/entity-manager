#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>

class ConfigCache
{
  public:
    explicit ConfigCache(const std::filesystem::path& configurationOutDir);

    const std::filesystem::path tempConfigDir;
    const std::filesystem::path lastConfiguration;

    const std::filesystem::path configurationOutDir;

    bool writeJsonFiles(const nlohmann::json& systemConfiguration);

    void removeCurrentConfiguration();
    bool currentConfigurationExists();
    void backupOldConfiguration();

  private:
    const std::filesystem::path currentConfiguration;
};
