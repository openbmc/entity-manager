#include "configuration.hpp"

#include "utils.hpp"

#include <nlohmann/json.hpp>
#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <string>
#include <vector>

namespace configuration
{
// writes output files to persist data
bool writeJsonFiles(const nlohmann::json& systemConfiguration)
{
    std::filesystem::create_directory(configurationOutDir);
    std::ofstream output(currentConfiguration);
    if (!output.good())
    {
        return false;
    }
    output << systemConfiguration.dump(4);
    output.close();
    return true;
}

// reads json files out of the filesystem
bool loadConfigurations(std::list<nlohmann::json>& configurations)
{
    // find configuration files
    std::vector<std::filesystem::path> jsonPaths;
    if (!findFiles(
            std::vector<std::filesystem::path>{configurationDirectory,
                                               hostConfigurationDirectory},
            R"(.*\.json)", jsonPaths))
    {
        std::cerr << "Unable to find any configuration files in "
                  << configurationDirectory << "\n";
        return false;
    }

    std::ifstream schemaStream(
        std::string(schemaDirectory) + "/" + globalSchema);
    if (!schemaStream.good())
    {
        std::cerr
            << "Cannot open schema file,  cannot validate JSON, exiting\n\n";
        std::exit(EXIT_FAILURE);
        return false;
    }
    nlohmann::json schema =
        nlohmann::json::parse(schemaStream, nullptr, false, true);
    if (schema.is_discarded())
    {
        std::cerr
            << "Illegal schema file detected, cannot validate JSON, exiting\n";
        std::exit(EXIT_FAILURE);
        return false;
    }

    for (auto& jsonPath : jsonPaths)
    {
        std::ifstream jsonStream(jsonPath.c_str());
        if (!jsonStream.good())
        {
            std::cerr << "unable to open " << jsonPath.string() << "\n";
            continue;
        }
        auto data = nlohmann::json::parse(jsonStream, nullptr, false, true);
        if (data.is_discarded())
        {
            std::cerr << "syntax error in " << jsonPath.string() << "\n";
            continue;
        }
        /*
        * todo(james): reenable this once less things are in flight
        *
        if (!validateJson(schema, data))
        {
            std::cerr << "Error validating " << jsonPath.string() << "\n";
            continue;
        }
        */

        if (data.type() == nlohmann::json::value_t::array)
        {
            for (auto& d : data)
            {
                configurations.emplace_back(d);
            }
        }
        else
        {
            configurations.emplace_back(data);
        }
    }
    return true;
}

void deriveNewConfiguration(const nlohmann::json& oldConfiguration,
                            nlohmann::json& newConfiguration)
{
    for (auto it = newConfiguration.begin(); it != newConfiguration.end();)
    {
        auto findKey = oldConfiguration.find(it.key());
        if (findKey != oldConfiguration.end())
        {
            it = newConfiguration.erase(it);
        }
        else
        {
            it++;
        }
    }
}

bool validateJson(const nlohmann::json& schemaFile, const nlohmann::json& input)
{
    valijson::Schema schema;
    valijson::SchemaParser parser;
    valijson::adapters::NlohmannJsonAdapter schemaAdapter(schemaFile);
    parser.populateSchema(schemaAdapter, schema);
    valijson::Validator validator;
    valijson::adapters::NlohmannJsonAdapter targetAdapter(input);
    return validator.validate(schema, targetAdapter, nullptr);
}
} // namespace configuration
