// SPDX-License-Identifier: Apache-2.0
#include "config_validation.hpp"

#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validation_results.hpp>
#include <valijson/validator.hpp>

#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace config_validation
{

nlohmann::json loadJson(const fs::path& path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Cannot open " + path.string());
    }

    try
    {
        return nlohmann::json::parse(file, nullptr, true, true);
    }
    catch (const nlohmann::json::parse_error& error)
    {
        throw std::runtime_error(
            "Cannot parse " + path.string() + ": " + error.what());
    }
}

namespace
{

const nlohmann::json* fetchSchemaDocument(const fs::path& schemaDirectory,
                                          const std::string& uri)
{
    fs::path reference(uri);
    if (reference.is_absolute() || uri.contains("://"))
    {
        return nullptr;
    }
    fs::path target = fs::weakly_canonical(schemaDirectory / reference);
    fs::path relative = target.lexically_relative(schemaDirectory);
    if (relative.empty() || *relative.begin() == "..")
    {
        return nullptr;
    }
    return new nlohmann::json(loadJson(target));
}

void freeSchemaDocument(const nlohmann::json* document)
{
    delete document;
}

template <typename ValijsonError>
std::string errorPath(const ValijsonError& error)
{
    if constexpr (requires { error.jsonPointer; })
    {
        return error.jsonPointer;
    }
    else
    {
        // Older Valijson releases only provide legacy context segments.
        std::string path;
        for (const auto& segment : error.context)
        {
            path += segment;
        }
        return path;
    }
}

} // namespace

struct Validator::Impl
{
    valijson::Schema schema;
};

Validator::Validator(const fs::path& schemaPath) :
    impl(std::make_unique<Impl>())
{
    const fs::path schemaDirectory = fs::canonical(schemaPath).parent_path();
    const nlohmann::json root = loadJson(schemaPath);
    valijson::adapters::NlohmannJsonAdapter adapter(root);

    // Valijson owns referenced documents until schema compilation completes.
    valijson::SchemaParser parser;
    parser.populateSchema(
        adapter, impl->schema,
        std::bind_front(fetchSchemaDocument, std::cref(schemaDirectory)),
        freeSchemaDocument);
}

Validator::~Validator() = default;

bool Validator::validate(const nlohmann::json& document,
                         std::vector<Error>* errors) const
{
    Session session(*this);
    return session.validate(document, errors);
}

struct Session::Impl
{
    explicit Impl(const valijson::Schema& schema) : schema(schema) {}

    const valijson::Schema& schema;
    valijson::Validator validator;
};

Session::Session(const Validator& validator) :
    impl(std::make_unique<Impl>(validator.impl->schema))
{}

Session::~Session() = default;

bool Session::validate(const nlohmann::json& document,
                       std::vector<Error>* errors)
{
    valijson::adapters::NlohmannJsonAdapter adapter(document);
    if (errors != nullptr)
    {
        valijson::ValidationResults results;
        const bool valid =
            impl->validator.validate(impl->schema, adapter, &results);
        for (const auto& error : results)
        {
            errors->push_back({errorPath(error), error.description});
        }
        return valid;
    }
    return impl->validator.validate(impl->schema, adapter, nullptr);
}

} // namespace config_validation
