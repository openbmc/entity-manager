#pragma once

#include <nlohmann/json.hpp>

#include <list>
#include <set>

namespace configuration
{
constexpr const char* globalSchema = "global.json";
constexpr const char* hostConfigurationDirectory = SYSCONF_DIR "configurations";
constexpr const char* configurationDirectory = PACKAGE_DIR "configurations";
constexpr const char* currentConfiguration = "/var/configuration/system.json";
constexpr const char* schemaDirectory = PACKAGE_DIR "configurations/schemas";

bool writeJsonFiles(const nlohmann::json& systemConfiguration);

bool loadConfigurations(std::list<nlohmann::json>& configurations);

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

std::set<std::string> getProbeInterfaces();

} // namespace configuration
