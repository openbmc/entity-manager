#pragma once

#include <nlohmann/json.hpp>

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

    // This will be an hash (integer) in almost all cases
    std::string boardId;

    std::optional<uint64_t> exposesIndex = std::nullopt;
    std::optional<std::string> propertyName = std::nullopt;

    std::optional<uint64_t> arrayIndex = std::nullopt;
};
