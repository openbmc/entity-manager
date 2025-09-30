// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <span>

/**
 * @brief Decompress gzip data from memory using zlib
 *
 * @param compressed The compressed data as a vector of bytes
 * @return std::string The decompressed data as a string, empty on error
 */
std::string decompressGzip(std::span<uint8_t> compressed);

/**
 * @brief Get a specific node value from XML string using XPath
 *
 * @param xml The XML content as a string
 * @param nodeName The XPath expression to find the node
 * @return std::string The node value, empty on error
 */
std::string getNodeFromXml(const std::string& xml, const char* nodeName);
