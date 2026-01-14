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
        // check that the key is a number
        char* endptr = nullptr;
        long int value = std::strtol(k.c_str(), &endptr, 10);
        if (endptr == k.c_str() || *endptr != '\0')
        {
            return std::nullopt;
        }

        // check that the value is an object
        const auto* obj = v.get_ptr<const nlohmann::json::object_t*>();

        if (obj == nullptr)
        {
            continue;
        }

        config[value] = *obj;
    }

    return config;
}
