#pragma once

#include "config_pointer.hpp"
#include "em_config.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

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

bool writeJsonFiles(const nlohmann::json& systemConfiguration);

template <typename JsonType>
bool setJsonFromPointer(const ConfigPointer& configPtr, const JsonType& value,
                        nlohmann::json& systemConfiguration)
{
    try
    {
        return configPtr.write(value, systemConfiguration);
    }
    catch (const nlohmann::json::out_of_range&)
    {
        return false;
    }
}

void deriveNewConfiguration(const nlohmann::json& oldConfiguration,
                            nlohmann::json& newConfiguration);

bool validateJson(const nlohmann::json& schemaFile,
                  const nlohmann::json& input);
