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
        EMConfig& board = systemConfiguration.at(boardId);
        nlohmann::json newValue = value;
        nlohmann::json::object_t* record = nullptr;
        if (exposesIndex)
        {
            if (board.exposesRecords.size() <= *exposesIndex)
            {
                lg2::error("error: config ptr: invalid exposes index {INDEX}",
                           "INDEX", *exposesIndex);
                return false;
            }
            record = &board.exposesRecords[*exposesIndex];
        }
        if (!propertyName)
        {
            if (record != nullptr)
            {
                if (newValue.is_null())
                {
                    // Keep the slot so pointers to later exposes stay valid.
                    (*record)["Status"] = "disabled";
                    return true;
                }
                const auto* object =
                    newValue
                        .template get_ptr<const nlohmann::json::object_t*>();
                if (object == nullptr)
                {
                    return false;
                }
                *record = *object;
                return true;
            }
            auto parsed = EMConfig::fromJson(newValue);
            if (!parsed)
            {
                return false;
            }
            board = std::move(*parsed);
            return true;
        }

        if (record == nullptr && !memberName && !arrayIndex &&
            (*propertyName == "Name" || *propertyName == "Type"))
        {
            const auto* str = newValue.template get_ptr<const std::string*>();
            if (str == nullptr)
            {
                return false;
            }
            (*propertyName == "Name" ? board.name : board.type) = *str;
            return true;
        }

        if (record == nullptr)
        {
            auto it = board.extraInterfaces.find(*propertyName);
            if (it == board.extraInterfaces.end())
            {
                return false;
            }
            if (!memberName && !arrayIndex)
            {
                if (newValue.is_null())
                {
                    board.extraInterfaces.erase(it);
                    return true;
                }
                const auto* object =
                    newValue
                        .template get_ptr<const nlohmann::json::object_t*>();
                if (object == nullptr)
                {
                    return false;
                }
                it->second = *object;
                return true;
            }
            record = &it->second;
        }

        if (!record->contains(*propertyName) && exposesIndex)
        {
            lg2::error("error: config ptr: property {NAME} not found", "NAME",
                       *propertyName);
            return false;
        }
        nlohmann::json* target = nullptr;
        if (exposesIndex)
        {
            target = &record->at(*propertyName);
        }
        else if (memberName)
        {
            if (!record->contains(*memberName))
            {
                return false;
            }
            target = &record->at(*memberName);
        }
        if (arrayIndex)
        {
            if (target == nullptr || !target->is_array() ||
                target->size() <= *arrayIndex)
            {
                return false;
            }
            target = &(*target)[*arrayIndex];
        }
        if (memberName && exposesIndex)
        {
            if (target == nullptr || !target->is_object() ||
                !target->contains(*memberName))
            {
                return false;
            }
            target = &(*target)[*memberName];
        }
        if (target == nullptr)
        {
            return false;
        }
        *target = std::move(newValue);
        return true;
    }

    // This will be an hash (integer) in almost all cases
    std::string boardId;

    std::optional<uint64_t> exposesIndex = std::nullopt;
    std::optional<std::string> propertyName = std::nullopt;

    std::optional<uint64_t> arrayIndex = std::nullopt;
    // A property on an interface object or an object within an array.
    std::optional<std::string> memberName = std::nullopt;
};
