#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <flat_map>
#include <optional>
#include <string>

using SystemConfiguration = std::flat_map<uint64_t, nlohmann::json::object_t>;

std::optional<SystemConfiguration> systemConfigurationFromJson(
    const nlohmann::json& json);

// This structure is used instead of a json pointer.
// Which helps to avoid forcing us to use json data structures.
struct ConfigPointer
{
  public:
    explicit ConfigPointer(uint64_t boardIdIn) : boardId(boardIdIn) {}
    explicit ConfigPointer(uint64_t boardIdIn, uint64_t exposesIndexIn) :
        boardId(boardIdIn), exposesIndex(exposesIndexIn)
    {}
    explicit ConfigPointer(uint64_t boardIdIn, uint64_t exposesIndexIn,
                           const std::string& propNameIn) :
        boardId(boardIdIn), exposesIndex(exposesIndexIn),
        propertyName(propNameIn)
    {}

    // This is for properties directly on a board config and not part of an
    // exposes record
    explicit ConfigPointer(uint64_t boardIdIn, const std::string& propNameIn) :
        boardId(boardIdIn), propertyName(propNameIn)
    {}

    // This represents indexing into a board, exposes record, config record, and
    // array element within
    explicit ConfigPointer(uint64_t boardIdIn, uint64_t exposesIndexIn,
                           const std::string& propNameIn,
                           uint64_t arrayIndexIn) :
        boardId(boardIdIn), exposesIndex(exposesIndexIn),
        propertyName(propNameIn), arrayIndex(arrayIndexIn)
    {}

    uint64_t boardId{};
    std::optional<uint64_t> exposesIndex = std::nullopt;
    std::optional<std::string> propertyName = std::nullopt;

    std::optional<uint64_t> arrayIndex = std::nullopt;
};
