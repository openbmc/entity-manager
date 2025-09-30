// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <optional>


std::optional<std::string> gzipInflate(std::span<uint8_t> compressedBytes);

std::string getNodeFromXml(const std::string& xml, const char* nodeName);
