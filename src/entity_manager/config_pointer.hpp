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
    bool write(const nlohmann::json& value,
               nlohmann::json& systemConfiguration) const;

    // This will be an hash (integer) in almost all cases
    std::string boardId;

    std::optional<uint64_t> exposesIndex;
    std::optional<std::string> propertyName;

    std::optional<uint64_t> arrayIndex;
    // A property on an interface object or an object within an array.
    std::optional<std::string> memberName;

  private:
    nlohmann::json* findRecord(nlohmann::json& board) const;
    nlohmann::json* findValue(nlohmann::json& record) const;
};
