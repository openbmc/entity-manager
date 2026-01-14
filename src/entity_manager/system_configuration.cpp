#include "system_configuration.hpp"

#include <nlohmann/json.hpp>

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

        config.insert_or_assign(k, *obj);
    }

    return config;
}
