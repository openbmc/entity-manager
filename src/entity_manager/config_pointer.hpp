#pragma once

#include "system_configuration.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <cstdint>
#include <optional>
#include <string>

// This structure is used instead of a json pointer.
// Which helps to avoid forcing us to use json data structures.
struct ConfigPointer
{
  public:
    explicit ConfigPointer(const std::string& boardIdIn);
    explicit ConfigPointer(const std::string& boardIdIn,
                           uint64_t exposesIndexIn);
    explicit ConfigPointer(const std::string& boardIdIn,
                           uint64_t exposesIndexIn,
                           const std::string& propNameIn);

    // This is for properties directly on a board config and not part of an
    // exposes record
    explicit ConfigPointer(const std::string& boardIdIn,
                           const std::string& propNameIn);

    // This represents indexing into a board, exposes record, config record, and
    // array element within
    explicit ConfigPointer(const std::string& boardIdIn,
                           uint64_t exposesIndexIn,
                           const std::string& propNameIn,
                           uint64_t arrayIndexIn);

    // Builder-style API (implemented as-needed)

    ConfigPointer withExposesIndexAndName(uint64_t index,
                                          const std::string& name) const;

    ConfigPointer withArrayIndex(uint64_t arrayIndex) const;

    ConfigPointer withName(const std::string& name) const;

    // @brief writes configuration at the pointed-to location
    // @returns false on error
    template <typename JsonType>
    bool write(const JsonType& value,
               SystemConfiguration& systemConfiguration) const
    {
        lg2::debug("config ptr: writing value: {VALUE}", "VALUE",
                   nlohmann::json(value));

        if (!systemConfiguration.contains(boardId))
        {
            lg2::error("error: config ptr: board id {ID} not found", "ID",
                       boardId);
            return false;
        }
        nlohmann::json::object_t& ref = systemConfiguration.at(boardId);
        if (!exposesIndex.has_value())
        {
            if (!propertyName.has_value())
            {
                nlohmann::json valueJson = value;
                if (!valueJson.is_object())
                {
                    lg2::error("error: config ptr: value is not an object");
                    return false;
                }
                ref = *valueJson
                           .template get_ptr<const nlohmann::json::object_t*>();
                return true;
            }
            ref[propertyName.value()] = value;
            return true;
        }
        const uint64_t exposesIndexValue = exposesIndex.value();
        if (!ref.contains("Exposes"))
        {
            lg2::error("error: config ptr: missing 'Exposes' property");
            return false;
        }
        nlohmann::json& exposesJson = ref["Exposes"];
        if (!exposesJson.is_array())
        {
            lg2::error("error: config ptr: 'Exposes' is not an array");
            return false;
        }
        if (exposesJson.size() <= exposesIndexValue)
        {
            lg2::error("error: config ptr: exposes index {INDEX} out of bounds",
                       "INDEX", exposesIndexValue);
            return false;
        }
        nlohmann::json& record = exposesJson[exposesIndexValue];
        if (!propertyName.has_value())
        {
            record = value;
            return true;
        }
        const std::string& propNameValue = propertyName.value();
        if (!record.contains(propNameValue))
        {
            lg2::error("error: config ptr: could not set property {NAME}",
                       "NAME", propNameValue);
            return false;
        }
        if (!arrayIndex.has_value())
        {
            record[propNameValue] = value;
            return true;
        }
        const uint64_t arrayIndexValue = arrayIndex.value();
        if (!record[propNameValue].is_array())
        {
            lg2::error("error: config ptr: property {NAME} is not an array",
                       "NAME", propNameValue);
            return false;
        }
        nlohmann::json& arrayJson = record[propNameValue];
        if (arrayJson.size() <= arrayIndexValue)
        {
            lg2::error("error: config ptr: array index {INDEX} out of bounds",
                       "INDEX", arrayIndexValue);
            return false;
        }
        record[propNameValue][arrayIndexValue] = value;
        return true;
    }

    // This will be an hash (integer) in almost all cases
    std::string boardId;

    std::optional<uint64_t> exposesIndex = std::nullopt;
    std::optional<std::string> propertyName = std::nullopt;

    std::optional<uint64_t> arrayIndex = std::nullopt;
};
