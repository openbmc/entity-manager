#include "system_configuration.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <optional>

std::optional<SystemConfiguration> systemConfigurationFromJson(
    const nlohmann::json& json)
{
    SystemConfiguration config;

    const auto* mapping = json.get_ptr<const nlohmann::json::object_t*>();

    if (mapping == nullptr)
    {
        return std::nullopt;
    }

    for (const auto& [k, v] : *mapping)
    {
        // check that the value is an object
        const nlohmann::json::object_t* obj =
            v.get_ptr<const nlohmann::json::object_t*>();

        if (obj == nullptr)
        {
            continue;
        }

        std::optional<EMConfig> optEMConfig = EMConfig::fromJson(*obj);

        if (!optEMConfig.has_value())
        {
            lg2::error("error parsing SystemConfiguration");
            continue;
        }

        config.insert_or_assign(k, optEMConfig.value());
    }

    return config;
}
