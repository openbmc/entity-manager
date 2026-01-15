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
    ConfigPointer(const std::string& boardIdIn, uint64_t exposesIndexIn);
    ConfigPointer(const std::string& boardIdIn, uint64_t exposesIndexIn,
                  const std::string& propNameIn);

    // This is for properties directly on a board config and not part of an
    // exposes record
    ConfigPointer(const std::string& boardIdIn, const std::string& propNameIn);

    // This represents indexing into a board, exposes record, config record, and
    // array element within
    ConfigPointer(const std::string& boardIdIn, uint64_t exposesIndexIn,
                  const std::string& propNameIn, uint64_t arrayIndexIn);

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
        lg2::debug("config ptr: writing value: {VALUE}", "VALUE",
                   nlohmann::json(value));

        auto targetIt = systemConfiguration.find(boardId);
        if (targetIt == systemConfiguration.end())
        {
            lg2::error("error: config ptr: board id {ID} not found", "ID",
                       boardId);
            return false;
        }
        nlohmann::json* target = &(*targetIt);
        if (exposesIndex)
        {
            auto it = target->find("Exposes");

            if (it == target->end() || !(*it).is_array() ||
                (*it).size() <= *exposesIndex)
            {
                lg2::error("error: config ptr: invalid exposes index {INDEX}",
                           "INDEX", *exposesIndex);
                return false;
            }
            target = &(*it)[*exposesIndex];
        }
        if (propertyName)
        {
            auto it = target->find(*propertyName);
            if (it == target->end())
            {
                lg2::error("error: config ptr: property {NAME} not found",
                           "NAME", *propertyName);
                return false;
            }
            target = &(*it);
        }
        if (arrayIndex)
        {
            if (!target->is_array() || target->size() <= *arrayIndex)
            {
                lg2::error("error: config ptr: invalid array index {INDEX}",
                           "INDEX", *arrayIndex);
                return false;
            }
            target = &(*target)[*arrayIndex];
        }
        if (memberName)
        {
            auto it = target->find(*memberName);
            if (it == target->end())
            {
                lg2::error("error: config ptr: property {NAME} not found",
                           "NAME", *memberName);
                return false;
            }
            target = &(*it);
        }
        *target = value;
        return true;
    }

    // This will be an hash (integer) in almost all cases
    std::string boardId;

    std::optional<uint64_t> exposesIndex;
    std::optional<std::string> propertyName;

    std::optional<uint64_t> arrayIndex;
    // A property on an interface object or an object within an array.
    std::optional<std::string> memberName;
};
