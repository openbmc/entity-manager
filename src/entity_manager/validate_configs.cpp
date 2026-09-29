// SPDX-License-Identifier: Apache-2.0
#include "config_validation.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{

struct Options
{
    fs::path schema = fs::path(EM_SOURCE_ROOT) / "schemas/global.json";
    fs::path configDirectory = fs::path(EM_SOURCE_ROOT) / "configurations";
    std::vector<fs::path> configs;
    bool keepGoing = false;
    bool verbose = false;
    bool showHelp = false;
};

void printUsage()
{
    lg2::info(
        "Usage: validate-configs [options]\n"
        "  -s, --schema FILE          Root JSON schema\n"
        "  -c, --config FILE          Configuration (repeatable; defaults to all)\n"
        "  -k, --continue             Continue after a validation failure\n"
        "  -v, --verbose              Print validation details\n"
        "  -h, --help                 Show this help");
}

Options parseOptions(std::span<char*> arguments)
{
    Options options;
    for (size_t index = 1; index < arguments.size(); ++index)
    {
        std::string_view argument(arguments[index]);
        if (argument == "-h" || argument == "--help")
        {
            options.showHelp = true;
            return options;
        }
        if (argument == "-k" || argument == "--continue")
        {
            options.keepGoing = true;
            continue;
        }
        if (argument == "-v" || argument == "--verbose")
        {
            options.verbose = true;
            continue;
        }
        if (argument != "-s" && argument != "--schema" && argument != "-c" &&
            argument != "--config")
        {
            throw std::invalid_argument(
                "Unknown option: " + std::string(argument));
        }
        if (index + 1 >= arguments.size())
        {
            throw std::invalid_argument(
                "Missing value for " + std::string(argument));
        }
        std::string_view value(arguments[++index]);
        if (argument == "-s" || argument == "--schema")
        {
            options.schema = value;
        }
        else if (argument == "-c" || argument == "--config")
        {
            options.configs.emplace_back(value);
        }
    }
    return options;
}

std::vector<fs::path> findConfigs(const fs::path& directory)
{
    std::vector<fs::path> configs;
    for (const auto& entry : fs::recursive_directory_iterator(directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
        {
            configs.push_back(entry.path());
        }
    }
    std::ranges::sort(configs);
    return configs;
}

struct Result
{
    bool valid = false;
    std::vector<config_validation::Error> errors;
    std::string failure;
};

std::vector<Result> validateConfigs(
    const Options& options, const config_validation::Validator& validator)
{
    std::vector<Result> results(options.configs.size());
    for (size_t index = 0; index < options.configs.size(); ++index)
    {
        Result& result = results[index];
        try
        {
            nlohmann::json document =
                config_validation::loadJson(options.configs[index]);
            result.valid = validator.validate(
                document, options.verbose ? &result.errors : nullptr);
        }
        catch (const std::exception& error)
        {
            result.failure = error.what();
        }
        if (!options.keepGoing && (!result.failure.empty() || !result.valid))
        {
            break;
        }
    }
    return results;
}

int reportResults(const Options& options, const std::vector<Result>& results)
{
    bool failed = false;
    for (size_t index = 0; index < options.configs.size(); ++index)
    {
        const fs::path& path = options.configs[index];
        const Result& result = results[index];
        if (!result.failure.empty())
        {
            lg2::error("Failed to validate configuration {CONFIG}: {ERR}",
                       "CONFIG", path.string(), "ERR", result.failure);
            return 2;
        }
        if (!result.valid)
        {
            failed = true;
            lg2::error("Configuration {CONFIG} failed validation", "CONFIG",
                       path.string());
            if (options.verbose)
            {
                for (const auto& error : result.errors)
                {
                    lg2::error(
                        "Validation error in {CONFIG} at {JSON_PATH}: {DETAIL}",
                        "CONFIG", path.string(), "JSON_PATH", error.path,
                        "DETAIL", error.message);
                }
            }
            if (!options.keepGoing)
            {
                break;
            }
        }
    }
    return failed ? 1 : 0;
}

int run(std::span<char*> arguments)
{
    Options options = parseOptions(arguments);
    if (options.showHelp)
    {
        printUsage();
        return 0;
    }
    config_validation::Validator validator(options.schema);
    if (options.configs.empty())
    {
        options.configs = findConfigs(options.configDirectory);
    }

    // Collect before printing so diagnostics remain in configuration order.
    auto results = validateConfigs(options, validator);
    return reportResults(options, results);
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        return run({argv, static_cast<size_t>(argc)});
    }
    catch (const std::exception& error)
    {
        lg2::error("validate-configs: {ERR}", "ERR", error.what());
        return 2;
    }
}
