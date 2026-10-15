// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#include "config_cache.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

bool ConfigCache::writeJsonFiles(const nlohmann::json& systemConfiguration)
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

    std::string content = systemConfiguration.dump(4);
    if (content == lastWritten)
    {
        return true;
    }

    lg2::debug("writing system configuration to {PATH}", "PATH",
               currentConfiguration);

    std::ofstream output(currentConfiguration);
    if (!output.good())
    {
        return false;
    }
    output << content;
    output.close();
    if (output.fail())
    {
        return false;
    }
    lastWritten = std::move(content);
    return true;
}
