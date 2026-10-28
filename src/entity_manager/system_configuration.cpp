#include "system_configuration.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <optional>
#include <vector>

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

        // Older caches represented deleted Exposes records as null slots.
        // Remove them here without relaxing EMConfig parsing for source files.
        nlohmann::json cachedBoard = *obj;
        auto exposes = cachedBoard.find("Exposes");
        if (exposes != cachedBoard.end() && exposes->is_array())
        {
            std::erase(exposes->get_ref<nlohmann::json::array_t&>(),
                       nlohmann::json(nullptr));
        }

        std::optional<EMConfig> optEMConfig = EMConfig::fromJson(cachedBoard);

        if (!optEMConfig.has_value())
        {
            lg2::error("Error parsing cached board {BOARD}", "BOARD", k);
            continue;
        }

        config.insert_or_assign(k, optEMConfig.value());
    }

    return config;
}
