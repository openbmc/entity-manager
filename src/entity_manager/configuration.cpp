#include "configuration.hpp"

#include "perform_probe.hpp"
#include "probe_type.hpp"
#include "utils.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

Configuration::Configuration(
    const std::vector<std::filesystem::path>& configurationDirectories,
    const std::filesystem::path& schemaDirectory) :
    schemaDirectory(schemaDirectory),
    configurationDirectories(configurationDirectories)
{
    loadConfigurations();
    filterProbeInterfaces();
}

void Configuration::loadConfigurations()
{
    const auto start = std::chrono::steady_clock::now();

    // find configuration files
    std::vector<std::filesystem::path> jsonPaths;
    if (!findFiles(configurationDirectories, R"(.*\.json)", jsonPaths))
    {
        for (const auto& configurationDirectory : configurationDirectories)
        {
            lg2::error("Unable to find any configuration files in {DIR}", "DIR",
                       configurationDirectory);
        }
        return;
    }

    nlohmann::json schema;

    if constexpr (ENABLE_RUNTIME_VALIDATE_JSON)
    {
        std::ifstream schemaStream(schemaDirectory / "global.json");
        if (!schemaStream.good())
        {
            lg2::error(
                "Cannot open schema file,  cannot validate JSON, exiting");
            std::exit(EXIT_FAILURE);
            return;
        }
        schema = nlohmann::json::parse(schemaStream, nullptr, false, true);
        if (schema.is_discarded())
        {
            lg2::error(
                "Illegal schema file detected, cannot validate JSON, exiting");
            std::exit(EXIT_FAILURE);
            return;
        }
    }

    for (auto& jsonPath : jsonPaths)
    {
        std::ifstream jsonStream(jsonPath.c_str());
        if (!jsonStream.good())
        {
            lg2::error("unable to open {PATH}", "PATH", jsonPath.string());
            continue;
        }
        auto data = nlohmann::json::parse(jsonStream, nullptr, false, true);
        if (data.is_discarded())
        {
            lg2::error("syntax error in {PATH}", "PATH", jsonPath.string());
            continue;
        }

        if (ENABLE_RUNTIME_VALIDATE_JSON && !validateJson(schema, data))
        {
            lg2::error("Error validating {PATH}", "PATH", jsonPath.string());
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

    lg2::debug(
        "Finished loading {NCONFIGS} json configuration(s) from {NFILES} file(s) in {MILLIS}ms",
        "NCONFIGS", configurations.size(), "NFILES", jsonPaths.size(), "MILLIS",
        duration);
}

// Iterate over new configuration and erase items from old configuration.
void deriveNewConfiguration(const nlohmann::json& oldConfiguration,
                            nlohmann::json& newConfiguration)
{
    lg2::debug("deriving new configuration");

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
            lg2::error("configuration file missing probe: {PROBE}", "PROBE",
                       *it);
            it++;
            continue;
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
                lg2::error("Probe statement wasn't a string, can't parse");
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
    if (!EM_CACHE_CONFIGURATION)
    {
        return true;
    }

    std::error_code ec;
    std::filesystem::create_directory(configurationOutDir, ec);
    if (ec)
    {
        return false;
    }

    lg2::debug("writing system configuration to {PATH}", "PATH",
               currentConfiguration);

    std::ofstream output(currentConfiguration);
    if (!output.good())
    {
        return false;
    }
    output << systemConfiguration.dump(4);
    output.close();
    return true;
}
