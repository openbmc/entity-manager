#pragma once

#include "config_pointer.hpp"
#include "em_config.hpp"
#include "system_configuration.hpp"

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

bool writeJsonFiles(const SystemConfiguration& systemConfiguration);

template <typename JsonType>
bool setJsonFromPointer(const ConfigPointer& configPtr, const std::string& name,
                        const JsonType& value,
                        SystemConfiguration& systemConfiguration)
{
    try
    {
        if (!systemConfiguration.contains(configPtr.boardId))
        {
            return false;
        }
        nlohmann::json::object_t& ref =
            systemConfiguration.at(configPtr.boardId);

        if (!configPtr.exposesIndex.has_value())
        {
            ref[name] = value;
            return true;
        }

        const uint64_t exposesIndex = configPtr.exposesIndex.value();

        if (!ref.contains("Exposes"))
        {
            lg2::error("error: config: missing 'Exposes' property");
            return false;
        }

        nlohmann::json& exposesJson = ref["Exposes"];

        if (!exposesJson.is_array())
        {
            lg2::error("error: config: 'Exposes' is not an array");
            return false;
        }
        if (exposesJson.size() <= exposesIndex)
        {
            lg2::error("error: config pointer: exposes index out of bounds");
            return false;
        }

        nlohmann::json& record = exposesJson[exposesIndex];

        if (!configPtr.propertyName.has_value())
        {
            record = value;
            return true;
        }

        if (!record.contains(configPtr.propertyName.value()))
        {
            lg2::error("error: config ptr: could not set property {NAME}",
                       "NAME", configPtr.propertyName.value());
            return false;
        }

        record[configPtr.propertyName.value()] = value;

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
