#pragma once

#include "em_config.hpp"
#include "system_configuration.hpp"

#include <nlohmann/json.hpp>

#include <unordered_set>
#include <vector>

constexpr const char* currentConfiguration = "/var/configuration/system.json";

class Configuration
{
  public:
    explicit Configuration(
        const std::vector<std::filesystem::path>& configurationDirectories,
        const std::filesystem::path& schemaDirectory);
    std::unordered_set<std::string> probeInterfaces;
    std::vector<EMConfig> configurations;

    const std::filesystem::path schemaDirectory;

  protected:
    void loadConfigurations();
    void filterProbeInterfaces();

  private:
    std::vector<std::filesystem::path> configurationDirectories;
};

bool writeJsonFiles(const SystemConfiguration& systemConfiguration);

template <typename JsonType>
bool setJsonFromPointer(const uint64_t boardId, const uint64_t exposesIndex,
                        const std::string& name, const JsonType& value,
                        SystemConfiguration& systemConfiguration)
{
    try
    {
        nlohmann::json& ref =
            systemConfiguration[boardId]["Exposes"][exposesIndex][name];
        ref = value;
        return true;
    }
    catch (const nlohmann::json::out_of_range&)
    {
        return false;
    }
}

void deriveNewConfiguration(const SystemConfiguration& oldConfiguration,
                            SystemConfiguration& newConfiguration);

bool validateJson(const nlohmann::json& schemaFile,
                  const nlohmann::json& input);
