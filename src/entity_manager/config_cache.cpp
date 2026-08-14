// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#include "config_cache.hpp"

#include <phosphor-logging/lg2.hpp>

#include <fstream>

static void filterDynamicEntries(nlohmann::json& out)
{
    for (const auto& [boardId, board] : out.items())
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

    nlohmann::json out = systemConfiguration;
    std::string content = out.dump(4);
    if (content == lastWritten)
    {
        return true;
    }

    lg2::debug("writing system configuration to {PATH}", "PATH",
               currentConfiguration);

    filterDynamicEntries(out);
    content = out.dump(4);

    std::ofstream output(currentConfiguration);
    if (!output.good())
    {
        return false;
    }
    output << content;
    output.close();
    if (output.fail())
    {
        lastWritten.clear();
        return false;
    }
    lastWritten = std::move(content);
    return true;
}
