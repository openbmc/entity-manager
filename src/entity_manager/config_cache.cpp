// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#include "config_cache.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

bool ConfigCache::writeJsonFiles(const SystemConfiguration& systemConfiguration)
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
    nlohmann::json::object_t out;
    for (const auto& [key, value] : systemConfiguration)
    {
        out[key] = value;
    }
    output << nlohmann::json(out).dump(4);
    output.close();
    return true;
}
