// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#include "config_cache.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

static nlohmann::json filterDynamicEntries(nlohmann::json out)
{
    for (auto& [boardId, board] : out.items())
    {
        if (!board.contains("Exposes"))
        {
            continue;
        }
        for (auto& expose : board["Exposes"])
        {
            if (!expose.is_null() && expose.value(dynamicKey, false))
            {
                expose = nullptr;
            }
        }
    }
    return out;
}

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

    lg2::debug("writing system configuration to {PATH}", "PATH",
               currentConfiguration);

    nlohmann::json out = filterDynamicEntries(systemConfiguration);

    std::ofstream output(currentConfiguration);
    if (!output.good())
    {
        return false;
    }
    output << out.dump(4);
    output.close();
    return true;
}
