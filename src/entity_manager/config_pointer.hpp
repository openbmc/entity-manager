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
    bool write(const JsonType& value,
               SystemConfiguration& systemConfiguration) const
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
        nlohmann::json::object_t& board = targetIt->second;
        nlohmann::json* target = nullptr;
        if (exposesIndex)
        {
            auto exposes = board.find("Exposes");
            if (exposes == board.end() || !exposes->second.is_array() ||
                exposes->second.size() <= *exposesIndex)
            {
                lg2::error("error: config ptr: invalid exposes index {INDEX}",
                           "INDEX", *exposesIndex);
                return false;
            }
            target = &exposes->second[*exposesIndex];
        }
        if (propertyName)
        {
            nlohmann::json::object_t* object =
                target ? target->get_ptr<nlohmann::json::object_t*>() : &board;
            if (object == nullptr)
            {
                lg2::error("error: config ptr: property {NAME} not found",
                           "NAME", *propertyName);
                return false;
            }
            auto it = object->find(*propertyName);
            if (it == object->end())
            {
                lg2::error("error: config ptr: property {NAME} not found",
                           "NAME", *propertyName);
                return false;
            }
            target = &it->second;
        }
        if (arrayIndex)
        {
            if (target == nullptr || !target->is_array() ||
                target->size() <= *arrayIndex)
            {
                lg2::error("error: config ptr: invalid array index {INDEX}",
                           "INDEX", *arrayIndex);
                return false;
            }
            target = &(*target)[*arrayIndex];
        }
        if (memberName)
        {
            if (target == nullptr)
            {
                lg2::error("error: config ptr: property {NAME} not found",
                           "NAME", *memberName);
                return false;
            }
            auto it = target->find(*memberName);
            if (it == target->end())
            {
                lg2::error("error: config ptr: property {NAME} not found",
                           "NAME", *memberName);
                return false;
            }
            target = &(*it);
        }
        if (target != nullptr)
        {
            *target = value;
            return true;
        }
        nlohmann::json boardValue = value;
        const auto* object =
            boardValue.template get_ptr<const nlohmann::json::object_t*>();
        if (object == nullptr)
        {
            lg2::error("error: config ptr: board value is not an object");
            return false;
        }
        board = *object;
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
