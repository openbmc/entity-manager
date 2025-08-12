#include "configuration.hpp"

#include "perform_probe.hpp"
#include "phosphor-logging/lg2.hpp"
#include "utils.hpp"

#include <nlohmann/json.hpp>
#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

const std::regex illegalDbusMemberRegex("[^A-Za-z0-9_]");

Configuration::Configuration()
{
    loadConfigurations();
    filterProbeInterfaces();
}

void Configuration::loadConfigurations()
{
    const auto start = std::chrono::steady_clock::now();

    // find configuration files
    std::vector<std::filesystem::path> jsonPaths;
    if (!findFiles(
            std::vector<std::filesystem::path>{configurationDirectory,
                                               hostConfigurationDirectory},
            R"(.*\.json)", jsonPaths))
    {
        std::cerr << "Unable to find any configuration files in "
                  << configurationDirectory << "\n";
        return;
    }

    std::ifstream schemaStream(
        std::string(schemaDirectory) + "/" + globalSchema);
    if (!schemaStream.good())
    {
        std::cerr
            << "Cannot open schema file,  cannot validate JSON, exiting\n\n";
        std::exit(EXIT_FAILURE);
        return;
    }
    nlohmann::json schema =
        nlohmann::json::parse(schemaStream, nullptr, false, true);
    if (schema.is_discarded())
    {
        std::cerr
            << "Illegal schema file detected, cannot validate JSON, exiting\n";
        std::exit(EXIT_FAILURE);
        return;
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

        if (ENABLE_RUNTIME_VALIDATE_JSON && !validateJson(schema, data))
        {
            std::cerr << "Error validating " << jsonPath.string() << "\n";
            continue;
        }

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

    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - start)
                              .count();

    lg2::debug("Finished loading json configuration in {MILLIS}ms", "MILLIS",
               duration);
}

// Iterate over new configuration and erase items from old configuration.
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

// validates a given input(configuration) with a given json schema file.
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

// Extract the D-Bus interfaces to probe from the JSON config files.
void Configuration::filterProbeInterfaces()
{
    for (auto it = configurations.begin(); it != configurations.end();)
    {
        auto findProbe = it->find("Probe");
        if (findProbe == it->end())
        {
            std::cerr << "configuration file missing probe:\n " << *it << "\n";
            it++;
            continue;
        }

        auto findName = it->find("Name");
        if (findName == it->end())
        {
            error("Configuration file missing name");
            it = configurations.erase(it);
            continue;
        }

        std::string boardType;
        auto findBoardType = it->find("Type");
        if (findBoardType != it->end() &&
            findBoardType->type() == nlohmann::json::value_t::string)
        {
            boardType = findBoardType->get<std::string>();
            std::regex_replace(boardType.begin(), boardType.begin(),
                               boardType.end(), illegalDbusMemberRegex, "_");
        }
        else
        {
            debug("Unable to find type for {BOARDNAME}, reverting to Chassis",
                  "BOARDTYPE", findName->get<std::string>());
            configurations.at(*it)["Type"] = "Chassis";
        }

        nlohmann::json probeCommand;
        if ((*findProbe).type() != nlohmann::json::value_t::array)
        {
            probeCommand = nlohmann::json::array();
            probeCommand.push_back(*findProbe);
        }
        else
        {
            probeCommand = *findProbe;
        }

        for (const nlohmann::json& probeJson : probeCommand)
        {
            const std::string* probe = probeJson.get_ptr<const std::string*>();
            if (probe == nullptr)
            {
                std::cerr << "Probe statement wasn't a string, can't parse";
                continue;
            }
            // Skip it if the probe cmd doesn't contain an interface.
            if (probe::findProbeType(*probe))
            {
                continue;
            }

            // syntax requires probe before first open brace
            auto findStart = probe->find('(');
            if (findStart != std::string::npos)
            {
                std::string interface = probe->substr(0, findStart);
                probeInterfaces.emplace(interface);
            }
        }
        it++;
    }
}

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
