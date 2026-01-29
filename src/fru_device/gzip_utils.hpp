// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

std::optional<std::string> gzipInflate(
    std::span<const uint8_t> compressedBytes);

std::vector<std::string> getNodeFromXml(std::string_view xml,
                                        const char* nodeName);
