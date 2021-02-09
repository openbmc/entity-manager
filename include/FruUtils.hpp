/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
/// \file FruUtils.hpp

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

extern "C"
{
// Include for I2C_SMBUS_BLOCK_MAX
#include <linux/i2c.h>
}

enum class fruAreas
{
    fruAreaInternal = 0,
    fruAreaChassis,
    fruAreaBoard,
    fruAreaProduct,
    fruAreaMultirecord
};
inline fruAreas operator++(fruAreas& x)
{
    return x = static_cast<fruAreas>(std::underlying_type<fruAreas>::type(x) +
                                     1);
}

const std::vector<std::string> FRU_AREA_NAMES = {"INTERNAL", "CHASSIS", "BOARD",
                                                 "PRODUCT", "MULTIRECORD"};

inline constexpr size_t fruBlockSize =
    8; // FRU areas are measured in 8-byte blocks

using ReadBlockFunc =
    std::function<int64_t(int flag, int file, uint16_t address, uint16_t offset,
                          uint8_t length, uint8_t* outBuf)>;

/// \brief Read and validate FRU contents.
/// \param flag the flag required for raw i2c
/// \param file the open file handle
/// \param address the i2c device address
/// \param readBlock a read method
/// \param errorHelp and a helper string for failures
/// \return the FRU contents from the file
std::vector<uint8_t> readFRUContents(int flag, int file, uint16_t address,
                                     ReadBlockFunc readBlock,
                                     const std::string& errorHelp);

/// \brief Validate an IPMI FRU common header
/// \param blockData the bytes comprising the common header
/// \return true if valid
bool validateHeader(const std::array<uint8_t, I2C_SMBUS_BLOCK_MAX>& blockData);

/// \brief Get offset for a common header area
/// \param area - the area
/// \return the field offset
unsigned int getHeaderAreaFieldOffset(fruAreas area);

/// \brief Get name of FRU areas
/// \param area - the area
/// \return the name of Fru areas
std::string getFruAreaName(fruAreas area);

/// \brief verifies overlapping of other offsets against given offset area
/// \param fruBytes Start of Fru data
/// \param currentArea Index of current area offset to be compared
/// \param len Length of current area space
/// \return true on success
bool verifyOffset(const std::vector<uint8_t>& fruBytes, fruAreas currentArea,
                  uint8_t len);
