// SPDX-License-Identifier: Apache-2.0
#include "config_validation.hpp"

#include <sched.h>

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <atomic>
#include <charconv>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
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
    fs::path expectedFails;
    bool keepGoing = false;
    bool verbose = false;
    bool showHelp = false;
    std::optional<size_t> threads;
};

void printUsage()
{
    lg2::info(
        "Usage: validate-configs [options]\n"
        "  -s, --schema FILE          Root JSON schema\n"
        "  -c, --config FILE          Configuration (repeatable; defaults to all)\n"
        "  -e, --expected-fails FILE  List of expected invalid basenames\n"
        "  -k, --continue             Continue after an unexpected result\n"
        "  -v, --verbose              Print validation details\n"
        "  -t, --threads COUNT        Override available CPU count\n"
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
            argument != "--config" && argument != "-e" &&
            argument != "--expected-fails" && argument != "-t" &&
            argument != "--threads")
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
        else if (argument == "-e" || argument == "--expected-fails")
        {
            options.expectedFails = value;
        }
        else
        {
            size_t count = 0;
            auto [end,
                  error] = std::from_chars(value.begin(), value.end(), count);
            if (error != std::errc() || end != value.end() || count == 0)
            {
                throw std::invalid_argument(
                    "Invalid thread count: " + std::string(value));
            }
            options.threads = count;
        }
    }
    return options;
}

std::unordered_set<std::string> readExpectedFails(const fs::path& path)
{
    std::unordered_set<std::string> names;
    if (path.empty())
    {
        return names;
    }
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("Cannot open " + path.string());
    }
    std::string line;
    while (std::getline(input, line))
    {
        size_t begin = line.find_first_not_of(" \t\r\n");
        if (begin != std::string::npos)
        {
            size_t end = line.find_last_not_of(" \t\r\n");
            names.insert(line.substr(begin, end - begin + 1));
        }
    }
    return names;
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

size_t availableCpus()
{
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    if (sched_getaffinity(0, sizeof(cpus), &cpus) == 0)
    {
        return std::max(1, CPU_COUNT(&cpus));
    }
    return std::max(1U, std::thread::hardware_concurrency());
}

struct Result
{
    bool valid = false;
    std::vector<config_validation::Error> errors;
    std::string failure;
};

struct WorkerState
{
    WorkerState(const Options& options,
                const config_validation::Validator& validator,
                const std::unordered_set<std::string>& expectedFails,
                std::vector<Result>& results) :
        options(options), validator(validator), expectedFails(expectedFails),
        results(results), stopAfter(options.configs.size())
    {}

    const Options& options;
    const config_validation::Validator& validator;
    const std::unordered_set<std::string>& expectedFails;
    std::vector<Result>& results;
    std::atomic_size_t next{0};
    std::atomic_size_t stopAfter;
    std::mutex errorMutex;
    std::exception_ptr error;
};

void requestStop(WorkerState& state, size_t index)
{
    if (state.options.keepGoing)
    {
        return;
    }
    size_t current = state.stopAfter.load();
    while (index < current &&
           !state.stopAfter.compare_exchange_weak(current, index))
    {}
}

void validateWorker(WorkerState& state)
{
    try
    {
        config_validation::Session session(state.validator);
        while (true)
        {
            size_t index = state.next.fetch_add(1);
            if (index >= state.options.configs.size() ||
                index > state.stopAfter.load())
            {
                break;
            }
            Result& result = state.results[index];
            try
            {
                nlohmann::json document =
                    config_validation::loadJson(state.options.configs[index]);
                result.valid = session.validate(
                    document, state.options.verbose ? &result.errors : nullptr);
            }
            catch (const std::exception& error)
            {
                result.failure = error.what();
                requestStop(state, index);
                continue;
            }
            const bool expected = state.expectedFails.contains(
                state.options.configs[index].filename().string());
            if (result.valid == expected)
            {
                requestStop(state, index);
            }
        }
    }
    catch (...)
    {
        std::lock_guard lock(state.errorMutex);
        if (!state.error)
        {
            state.error = std::current_exception();
        }
    }
}

std::vector<Result> validateConfigs(
    const Options& options, const config_validation::Validator& validator,
    const std::unordered_set<std::string>& expectedFails)
{
    std::vector<Result> results(options.configs.size());
    const size_t workerCount = std::min(
        options.configs.size(), options.threads.value_or(availableCpus()));
    WorkerState state(options, validator, expectedFails, results);
    // jthread joins on destruction, including when thread creation fails.
    std::vector<std::jthread> workers;
    workers.reserve(workerCount);
    for (size_t i = 0; i < workerCount; ++i)
    {
        workers.emplace_back(validateWorker, std::ref(state));
    }
    workers.clear(); // Join before inspecting errors or returning results.
    if (state.error)
    {
        std::rethrow_exception(state.error);
    }
    return results;
}

int reportResults(const Options& options,
                  const std::unordered_set<std::string>& expectedFails,
                  const std::vector<Result>& results)
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
        const bool expected = expectedFails.contains(path.filename().string());
        if (result.valid == expected)
        {
            failed = true;
            if (result.valid)
            {
                lg2::error("Configuration {CONFIG} passed unexpectedly",
                           "CONFIG", path.string());
            }
            else
            {
                lg2::error("Configuration {CONFIG} failed validation", "CONFIG",
                           path.string());
            }
            if (options.verbose && !result.valid)
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
    auto expectedFails = readExpectedFails(options.expectedFails);
    config_validation::Validator validator(options.schema);
    if (options.configs.empty())
    {
        options.configs = findConfigs(options.configDirectory);
    }

    // Collect before printing so diagnostics remain in configuration order.
    auto results = validateConfigs(options, validator, expectedFails);
    return reportResults(options, expectedFails, results);
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
