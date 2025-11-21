#include "config_cache.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <fstream>
#include <string>

ConfigCache::ConfigCache(const std::filesystem::path& configurationOutDir) :
    tempConfigDir("/tmp/configuration"),
    lastConfiguration(tempConfigDir / "last.json"),
    configurationOutDir(configurationOutDir),
    currentConfiguration(configurationOutDir / "system.json")
{}

bool ConfigCache::writeJsonFiles(const nlohmann::json& systemConfiguration)
{
    if (!EM_CACHE_CONFIGURATION)
    {
        return true;
    }

    lg2::debug("Writing current configuration to {PATH}", "PATH",
               currentConfiguration);

    std::error_code ec;
    std::filesystem::create_directory(configurationOutDir, ec);
    if (ec)
    {
        return false;
    }
    std::ofstream output(currentConfiguration);
    if (!output.good())
    {
        return false;
    }
    output << systemConfiguration.dump(4);
    output.close();
    return true;
}

void ConfigCache::removeCurrentConfiguration()
{
    lg2::info("Clearing previous configuration at {PATH}", "PATH",
              currentConfiguration);
    std::error_code ec;
    std::filesystem::remove(currentConfiguration, ec);
}

bool ConfigCache::currentConfigurationExists()
{
    return std::filesystem::is_regular_file(currentConfiguration);
}

void ConfigCache::backupOldConfiguration()
{
    lg2::debug("Backing up old configuration to {PATH}", "PATH",
               lastConfiguration);

    // this file could just be deleted, but it's nice for debug
    std::filesystem::create_directory(tempConfigDir);
    std::filesystem::remove(lastConfiguration);
    std::filesystem::copy(currentConfiguration, lastConfiguration);
}
