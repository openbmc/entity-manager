#pragma once

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
    bool write(const JsonType& value, nlohmann::json& systemConfiguration) const
    {
        if (!systemConfiguration.contains(boardId))
        {
            return false;
        }
        nlohmann::json& ref = systemConfiguration.at(boardId);
        if (!exposesIndex.has_value())
        {
            if (!propertyName.has_value())
            {
                lg2::error("error: config ptr: missing property name");
                return false;
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
            lg2::error("error: config ptr: exposes index out of bounds");
            return false;
        }
        nlohmann::json& record = exposesJson[exposesIndexValue];
        if (!propertyName.has_value())
        {
            record = value;
            return true;
        }
        if (!record.contains(propertyName.value()))
        {
            lg2::error("error: config ptr: could not set property {NAME}",
                       "NAME", propertyName.value());
            return false;
        }
        record[propertyName.value()] = value;
        return true;
    }

    // This will be an hash (integer) in almost all cases
    std::string boardId;

    std::optional<uint64_t> exposesIndex = std::nullopt;
    std::optional<std::string> propertyName = std::nullopt;

    std::optional<uint64_t> arrayIndex = std::nullopt;
};
