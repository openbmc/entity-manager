#pragma once

#include <nlohmann/json.hpp>

#include <unordered_set>
#include <vector>

constexpr const char* globalSchema = "global.json";
constexpr const char* hostConfigurationDirectory = SYSCONF_DIR "configurations";
constexpr const char* configurationDirectory = PACKAGE_DIR "configurations";
constexpr const char* currentConfiguration = "/var/configuration/system.json";
constexpr const char* schemaDirectory = PACKAGE_DIR "schemas";

class Configuration
{
  public:
    explicit Configuration();
    std::unordered_set<std::string> probeInterfaces;
    std::vector<nlohmann::json> configurations;

  private:
    void loadConfigurations();
    void filterProbeInterfaces();
};

bool writeJsonFiles(const nlohmann::json& systemConfiguration);

template <typename JsonType>
bool setJsonFromPointer(const std::string& ptrStr, const JsonType& value,
                        nlohmann::json& systemConfiguration)
{
    try
    {
        nlohmann::json::json_pointer ptr(ptrStr);
        nlohmann::json& ref = systemConfiguration[ptr];
        ref = value;
        return true;
    }
    catch (const std::out_of_range&)
    {
        return false;
    }
}

void deriveNewConfiguration(const nlohmann::json& oldConfiguration,
                            nlohmann::json& newConfiguration);

bool validateJson(const nlohmann::json& schemaFile,
                  const nlohmann::json& input);
