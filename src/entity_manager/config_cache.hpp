// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#pragma once

#include "system_configuration.hpp"

#include <filesystem>

class ConfigCache
{
  public:
    explicit ConfigCache(std::filesystem::path outDir = "/var/configuration") :
        configurationOutDir(std::move(outDir)),
        currentConfiguration(configurationOutDir / "system.json"),
        versionHashFile(configurationOutDir / "version")
    {}

    // @returns false on error
    bool writeJsonFiles(const SystemConfiguration& systemConfiguration);

    const std::filesystem::path configurationOutDir;
    const std::filesystem::path currentConfiguration;
    const std::filesystem::path versionHashFile;
};
