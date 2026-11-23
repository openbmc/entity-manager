// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026

#pragma once

#include <sys/types.h>

#include <cstdint>
#include <filesystem>
#include <span>

enum class EepromWriteResult
{
    success,
    seekError,
    writeError,
};

EepromWriteResult writeEepromFile(int fd, const std::filesystem::path& path,
                                  off_t offset, std::span<const uint8_t> data);
