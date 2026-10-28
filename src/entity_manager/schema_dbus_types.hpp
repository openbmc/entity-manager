#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <flat_map>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace schema_dbus_types
{

// A value converted to the D-Bus type its schema names, ready to publish.
using DBusValue = std::variant<bool, uint8_t, int64_t, uint64_t, double,
                               std::string, std::vector<uint8_t>,
                               std::vector<int64_t>, std::vector<uint64_t>,
                               std::vector<double>, std::vector<std::string>>;

// The D-Bus types the schemas name for record properties with
// "x-dbus-type". The type a value has in a configuration file is not always
// the type it is published as: a number is often written as a string so that
// it can hold a template, and only the schema can say it is meant to be a
// number.
class SchemaDBusTypes
{
  public:
    SchemaDBusTypes() = default;

    // Reads every schema under schemaDirectory. A schema that cannot be read
    // contributes nothing, so its records are published as before.
    explicit SchemaDBusTypes(const std::filesystem::path& schemaDirectory);

    // Adds the record definitions of one schema. schemas holds every schema
    // by its path relative to the schema directory, to resolve a property
    // that refers to another definition.
    void addSchema(
        const nlohmann::json& schema, const std::string& schemaName,
        const std::flat_map<std::string, nlohmann::json, std::less<>>& schemas);

    // The D-Bus type a property of a record of recordType is published as,
    // if its schema names one.
    std::optional<std::string_view> find(std::string_view recordType,
                                         std::string_view property) const;

  private:
    std::flat_map<std::string,
                  std::flat_map<std::string, std::string, std::less<>>,
                  std::less<>>
        types;
};

// Converts a configuration value to dbusType, or returns std::nullopt if it
// cannot be published as that type. A string is converted to a number the
// way template substitution converts one, so a value that is still a string
// here was neither a number nor a template that resolved to one.
std::optional<DBusValue> toDBusValue(std::string_view dbusType,
                                     const nlohmann::json& value);

} // namespace schema_dbus_types
