// SPDX-License-Identifier: Apache-2.0
#include "schema_defaults.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <fstream>

namespace
{
using Json = nlohmann::json;
using Documents = std::map<std::filesystem::path, Json>;

const Json& loadSchema(const std::filesystem::path& path, Documents& documents)
{
    auto [it, inserted] = documents.try_emplace(path);
    if (inserted)
    {
        std::ifstream stream(path);
        it->second = Json::parse(stream, nullptr, false, true);
        if (it->second.is_discarded())
        {
            lg2::error("Cannot read schema defaults from {PATH}", "PATH", path);
        }
    }
    return it->second;
}

// Resolve only the reference to the record definition, not its properties.
const Json* recordSchema(const Json& branch, const std::filesystem::path& index,
                         Documents& documents)
{
    const auto ref = branch.find("$ref");
    if (ref == branch.end())
    {
        return &branch;
    }
    const auto* text = ref->get_ptr<const std::string*>();
    if (text == nullptr)
    {
        return nullptr;
    }
    const auto hash = text->find('#');
    const auto file = text->substr(0, hash);
    const auto path =
        file.empty() ? index : (index.parent_path() / file).lexically_normal();
    const auto relative = path.lexically_relative(index.parent_path());
    if (file.find(':') != std::string::npos || relative.empty() ||
        *relative.begin() == "..")
    {
        return nullptr;
    }
    const auto& schema = loadSchema(path, documents);
    const auto fragment =
        hash == std::string::npos ? "" : text->substr(hash + 1);
    try
    {
        const Json::json_pointer pointer(fragment);
        return schema.contains(pointer) ? &schema.at(pointer) : nullptr;
    }
    catch (const Json::parse_error&)
    {
        return nullptr;
    }
}

bool isScalar(const Json& value)
{
    return value.is_boolean() || value.is_number() || value.is_string();
}

bool isPublishable(const Json& value)
{
    if (!value.is_array())
    {
        return isScalar(value);
    }
    if (value.empty() || !isScalar(value.front()))
    {
        return false;
    }
    const auto type = value.front().type();
    for (const auto& item : value)
    {
        if (item.type() != type)
        {
            return false;
        }
    }
    return true;
}

Json defaultsFor(const Json& record, const Json& properties)
{
    Json defaults = Json::object();
    const auto required = record.find("required");
    for (const auto& [name, property] : properties.items())
    {
        if (name == "Name" || name == "Type" || property.contains("$ref") ||
            (required != record.end() &&
             std::find(required->begin(), required->end(), name) !=
                 required->end()))
        {
            continue;
        }
        auto value = property.find("default");
        if (value != property.end() && isPublishable(*value))
        {
            defaults[name] = *value;
        }
    }
    return defaults;
}

bool isDirectSchema(const Json& schema)
{
    for (const auto* keyword :
         {"$ref", "allOf", "anyOf", "oneOf", "not", "if", "dependencies",
          "dependentRequired", "dependentSchemas"})
    {
        if (schema.contains(keyword))
        {
            return false;
        }
    }
    return true;
}
} // namespace

SchemaDefaults::SchemaDefaults(const std::filesystem::path& schemaDirectory)
{
    Documents documents;
    const auto index =
        (schemaDirectory / "exposes_record.json").lexically_normal();
    const auto& schema = loadSchema(index, documents);
    const Json::json_pointer branches("/$defs/ConfigSchema/oneOf");
    if (!schema.contains(branches) || !schema.at(branches).is_array())
    {
        return;
    }
    for (const auto& branch : schema.at(branches))
    {
        const auto* record = recordSchema(branch, index, documents);
        if (record == nullptr || !isDirectSchema(*record))
        {
            continue;
        }
        auto properties = record->find("properties");
        if (properties == record->end() || !properties->is_object())
        {
            continue;
        }
        auto type = properties->find("Type");
        if (type == properties->end() || !isDirectSchema(*type))
        {
            continue;
        }
        const auto defaults = defaultsFor(*record, *properties);
        auto name = type->find("const");
        if (name != type->end())
        {
            addType(*name, defaults);
        }
        else if (auto names = type->find("enum");
                 names != type->end() && names->is_array())
        {
            for (const auto& typeName : *names)
            {
                addType(typeName, defaults);
            }
        }
    }
}

void SchemaDefaults::addType(const Json& type, const Json& defaults)
{
    if (const auto* name = type.get_ptr<const std::string*>())
    {
        auto [it, inserted] = exposesDefaults.try_emplace(*name, defaults);
        if (!inserted)
        {
            // More than one branch defines this Type; do not choose a default.
            it->second.clear();
        }
    }
}

Json SchemaDefaults::exposes(const Json& record) const
{
    auto type = record.find("Type");
    if (type == record.end() || !type->is_string())
    {
        return record;
    }
    auto defaults = exposesDefaults.find(type->get_ref<const std::string&>());
    if (defaults == exposesDefaults.end() || defaults->second.empty())
    {
        return record;
    }

    Json effective = record;
    for (const auto& [name, value] : defaults->second.items())
    {
        if (!effective.contains(name))
        {
            effective[name] = value;
        }
    }
    return effective;
}
