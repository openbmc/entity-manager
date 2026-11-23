// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026

#pragma once

#include <sys/types.h>

#include <cstdint>
#include <span>

enum class EepromWriteResult
{
    success,
    seekError,
    writeError,
};

EepromWriteResult writeEepromFile(int fd, off_t offset,
                                  std::span<const uint8_t> data);
