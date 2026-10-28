#include "schema_dbus_types.hpp"

#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>
#include <system_error>
#include <type_traits>
#include <utility>

namespace schema_dbus_types
{

using Schemas = std::flat_map<std::string, nlohmann::json, std::less<>>;

// Follows a property's "$ref", which names a definition either in the same
// schema ("#/$defs/...") or in another one ("other.json#/$defs/..."), the
// other schema's path being relative to the schema directory or else to the
// referring schema's own directory.
static const nlohmann::json* resolveRef(const nlohmann::json& property,
                                        const std::string& schemaName,
                                        const Schemas& schemas)
{
    auto ref = property.find("$ref");
    if (ref == property.end() || !ref->is_string())
    {
        return &property;
    }

    const std::string& target = ref->get_ref<const std::string&>();
    const size_t hash = target.find('#');
    std::string file = target.substr(0, hash);
    const std::string pointer =
        hash == std::string::npos ? "" : target.substr(hash + 1);
    if (file.empty())
    {
        file = schemaName;
    }

    auto schema = schemas.find(file);
    if (schema == schemas.end())
    {
        schema = schemas.find(
            (std::filesystem::path(schemaName).parent_path() / file)
                .lexically_normal()
                .generic_string());
    }
    if (schema == schemas.end())
    {
        return nullptr;
    }

    try
    {
        return &schema->second.at(nlohmann::json::json_pointer(pointer));
    }
    catch (const nlohmann::json::exception&)
    {
        return nullptr;
    }
}

// The record Types a definition describes, from the const or enum its Type
// property is restricted to.
static std::vector<std::string> recordTypes(const nlohmann::json& properties)
{
    std::vector<std::string> types;
    auto type = properties.find("Type");
    if (type == properties.end() || !type->is_object())
    {
        return types;
    }

    auto constType = type->find("const");
    if (constType != type->end() && constType->is_string())
    {
        types.emplace_back(constType->get<std::string>());
    }
    auto enumTypes = type->find("enum");
    if (enumTypes != type->end() && enumTypes->is_array())
    {
        for (const auto& enumType : *enumTypes)
        {
            if (enumType.is_string())
            {
                types.emplace_back(enumType.get<std::string>());
            }
        }
    }
    return types;
}

SchemaDBusTypes::SchemaDBusTypes(const std::filesystem::path& schemaDirectory)
{
    Schemas schemas;
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(schemaDirectory, ec))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".json")
        {
            continue;
        }

        std::ifstream file(entry.path());
        nlohmann::json schema =
            nlohmann::json::parse(file, nullptr, false, true);
        if (schema.is_discarded())
        {
            lg2::error("Cannot parse schema {PATH} for D-Bus types", "PATH",
                       entry.path());
            continue;
        }
        schemas.emplace(
            entry.path().lexically_relative(schemaDirectory).generic_string(),
            std::move(schema));
    }

    for (const auto& [schemaName, schema] : schemas)
    {
        addSchema(schema, schemaName, schemas);
    }
}

void SchemaDBusTypes::addSchema(const nlohmann::json& schema,
                                const std::string& schemaName,
                                const Schemas& schemas)
{
    auto defs = schema.find("$defs");
    if (defs == schema.end() || !defs->is_object())
    {
        return;
    }

    for (const auto& def : *defs)
    {
        if (!def.is_object())
        {
            continue;
        }
        auto properties = def.find("properties");
        if (properties == def.end() || !properties->is_object())
        {
            continue;
        }
        const std::vector<std::string> recordTypeNames =
            recordTypes(*properties);
        if (recordTypeNames.empty())
        {
            continue;
        }

        for (const auto& [name, property] : properties->items())
        {
            const nlohmann::json* resolved = &property;
            if (!property.contains("x-dbus-type"))
            {
                resolved = resolveRef(property, schemaName, schemas);
            }
            if (resolved == nullptr || !resolved->is_object())
            {
                continue;
            }
            auto dbusType = resolved->find("x-dbus-type");
            if (dbusType == resolved->end() || !dbusType->is_string())
            {
                continue;
            }
            for (const std::string& type : recordTypeNames)
            {
                types[type].insert_or_assign(name,
                                             dbusType->get<std::string>());
            }
        }
    }
}

std::optional<std::string_view> SchemaDBusTypes::find(
    std::string_view recordType, std::string_view property) const
{
    auto record = types.find(recordType);
    if (record == types.end())
    {
        return std::nullopt;
    }
    auto dbusType = record->second.find(property);
    if (dbusType == record->second.end())
    {
        return std::nullopt;
    }
    return dbusType->second;
}

using Number = std::variant<uint64_t, int64_t, double>;

// A number, or a string that template substitution would have turned into
// one.
static std::optional<Number> toNumber(const nlohmann::json& value)
{
    if (value.is_number_unsigned())
    {
        return value.get<uint64_t>();
    }
    if (value.is_number_integer())
    {
        return value.get<int64_t>();
    }
    if (value.is_number_float())
    {
        return value.get<double>();
    }
    if (value.is_string())
    {
        std::optional<uint64_t> number =
            em_utils::parseAsNumber(value.get_ref<const std::string&>());
        if (number)
        {
            return *number;
        }
    }
    return std::nullopt;
}

template <typename T>
static std::optional<T> toScalar(const nlohmann::json& value)
{
    if constexpr (std::is_same_v<T, bool>)
    {
        if (!value.is_boolean())
        {
            return std::nullopt;
        }
        return value.get<bool>();
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        if (!value.is_string())
        {
            return std::nullopt;
        }
        return value.get<std::string>();
    }
    else
    {
        std::optional<Number> number = toNumber(value);
        if (!number)
        {
            return std::nullopt;
        }
        return std::visit(
            [](auto n) -> std::optional<T> {
                if constexpr (std::is_floating_point_v<T>)
                {
                    return static_cast<T>(n);
                }
                else if constexpr (std::is_floating_point_v<decltype(n)>)
                {
                    // A fraction has no integer to stand for it.
                    return std::nullopt;
                }
                else
                {
                    if (!std::in_range<T>(n))
                    {
                        return std::nullopt;
                    }
                    return static_cast<T>(n);
                }
            },
            *number);
    }
}

template <typename T>
static std::optional<DBusValue> convert(const nlohmann::json& value, bool array)
{
    if (!array)
    {
        std::optional<T> scalar = toScalar<T>(value);
        if (!scalar)
        {
            return std::nullopt;
        }
        return DBusValue{*scalar};
    }

    if constexpr (std::is_same_v<T, bool>)
    {
        // An array of booleans has no D-Bus representation here.
        return std::nullopt;
    }
    else
    {
        if (!value.is_array())
        {
            return std::nullopt;
        }
        std::vector<T> values;
        values.reserve(value.size());
        for (const auto& element : value)
        {
            std::optional<T> scalar = toScalar<T>(element);
            if (!scalar)
            {
                return std::nullopt;
            }
            values.emplace_back(*scalar);
        }
        return DBusValue{std::move(values)};
    }
}

std::optional<DBusValue> toDBusValue(std::string_view dbusType,
                                     const nlohmann::json& value)
{
    constexpr std::string_view arrayPrefix = "array[";
    bool array = false;
    if (dbusType.starts_with(arrayPrefix) && dbusType.ends_with(']'))
    {
        array = true;
        dbusType = dbusType.substr(arrayPrefix.size(),
                                   dbusType.size() - arrayPrefix.size() - 1);
    }

    if (dbusType == "byte")
    {
        return convert<uint8_t>(value, array);
    }
    if (dbusType == "int64")
    {
        return convert<int64_t>(value, array);
    }
    if (dbusType == "uint64")
    {
        return convert<uint64_t>(value, array);
    }
    if (dbusType == "double")
    {
        return convert<double>(value, array);
    }
    if (dbusType == "string")
    {
        return convert<std::string>(value, array);
    }
    if (dbusType == "boolean")
    {
        return convert<bool>(value, array);
    }
    return std::nullopt;
}

} // namespace schema_dbus_types
