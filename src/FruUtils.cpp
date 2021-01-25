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
/// \file FruUtils.cpp

#include "FruUtils.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

extern "C"
{
// Include for I2C_SMBUS_BLOCK_MAX
#include <linux/i2c.h>
}

static constexpr bool DEBUG = false;
constexpr size_t fruVersion = 1; // Current FRU spec version number is 1

bool validateHeader(const std::array<uint8_t, I2C_SMBUS_BLOCK_MAX>& blockData)
{
    // ipmi spec format version number is currently at 1, verify it
    if (blockData[0] != fruVersion)
    {
        if (DEBUG)
        {
            std::cerr << "FRU spec version " << (int)(blockData[0])
                      << " not supported. Supported version is "
                      << (int)(fruVersion) << "\n";
        }
        return false;
    }

    // verify pad is set to 0
    if (blockData[6] != 0x0)
    {
        if (DEBUG)
        {
            std::cerr << "PAD value in header is non zero, value is "
                      << (int)(blockData[6]) << "\n";
        }
        return false;
    }

    // verify offsets are 0, or don't point to another offset
    std::set<uint8_t> foundOffsets;
    for (int ii = 1; ii < 6; ii++)
    {
        if (blockData[ii] == 0)
        {
            continue;
        }
        auto inserted = foundOffsets.insert(blockData[ii]);
        if (!inserted.second)
        {
            return false;
        }
    }

    // validate checksum
    size_t sum = 0;
    for (int jj = 0; jj < 7; jj++)
    {
        sum += blockData[jj];
    }
    sum = (256 - sum) & 0xFF;

    if (sum != blockData[7])
    {
        if (DEBUG)
        {
            std::cerr << "Checksum " << (int)(blockData[7])
                      << " is invalid. calculated checksum is " << (int)(sum)
                      << "\n";
        }
        return false;
    }
    return true;
}

std::vector<uint8_t> readFRUContents(int flag, int file, uint16_t address,
                                     ReadBlockFunc readBlock,
                                     const std::string& errorHelp)
{
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;

    if (readBlock(flag, file, address, 0x0, 0x8, blockData.data()) < 0)
    {
        std::cerr << "failed to read " << errorHelp << "\n";
        return {};
    }

    // check the header checksum
    if (!validateHeader(blockData))
    {
        if (DEBUG)
        {
            std::cerr << "Illegal header " << errorHelp << "\n";
        }

        return {};
    }

    std::vector<uint8_t> device;
    device.insert(device.end(), blockData.begin(), blockData.begin() + 8);

    bool hasMultiRecords = false;
    size_t fruLength = fruBlockSize; // At least FRU header is present
    unsigned int prevOffset = 0;
    for (fruAreas area = fruAreas::fruAreaInternal;
         area <= fruAreas::fruAreaMultirecord; ++area)
    {
        // Offset value can be 255.
        unsigned int areaOffset = device[getHeaderAreaFieldOffset(area)];
        if (areaOffset == 0)
        {
            continue;
        }

        /* Check for offset order, as per Section 17 of FRU specification, FRU
         * information areas are required to be in order in FRU data layout
         * which means all offset value should be in increasing order or can be
         * 0 if that area is not present
         */
        if (areaOffset <= prevOffset)
        {
            std::cerr << "Fru area offsets are not in required order as per "
                         "Section 17 of Fru specification\n";
            return {};
        }
        prevOffset = areaOffset;

        // MultiRecords are different. area is not tracking section, it's
        // walking the common header.
        if (area == fruAreas::fruAreaMultirecord)
        {
            hasMultiRecords = true;
            break;
        }

        areaOffset *= fruBlockSize;

        if (readBlock(flag, file, address, static_cast<uint16_t>(areaOffset),
                      0x2, blockData.data()) < 0)
        {
            std::cerr << "failed to read " << errorHelp << "\n";
            return {};
        }

        // Ignore data type (blockData is already unsigned).
        size_t length = blockData[1] * fruBlockSize;
        areaOffset += length;
        fruLength = (areaOffset > fruLength) ? areaOffset : fruLength;
    }

    if (hasMultiRecords)
    {
        // device[area count] is the index to the last area because the 0th
        // entry is not an offset in the common header.
        unsigned int areaOffset =
            device[getHeaderAreaFieldOffset(fruAreas::fruAreaMultirecord)];
        areaOffset *= fruBlockSize;

        // the multi-area record header is 5 bytes long.
        constexpr size_t multiRecordHeaderSize = 5;
        constexpr uint8_t multiRecordEndOfListMask = 0x80;

        // Sanity hard-limit to 64KB.
        while (areaOffset < std::numeric_limits<uint16_t>::max())
        {
            // In multi-area, the area offset points to the 0th record, each
            // record has 3 bytes of the header we care about.
            if (readBlock(flag, file, address,
                          static_cast<uint16_t>(areaOffset), 0x3,
                          blockData.data()) < 0)
            {
                std::cerr << "failed to read " << errorHelp << "\n";
                return {};
            }

            // Ok, let's check the record length, which is in bytes (unsigned,
            // up to 255, so blockData should hold uint8_t not char)
            size_t recordLength = blockData[2];
            areaOffset += (recordLength + multiRecordHeaderSize);
            fruLength = (areaOffset > fruLength) ? areaOffset : fruLength;

            // If this is the end of the list bail.
            if ((blockData[1] & multiRecordEndOfListMask))
            {
                break;
            }
        }
    }

    // You already copied these first 8 bytes (the ipmi fru header size)
    fruLength -= std::min(fruBlockSize, fruLength);

    int readOffset = fruBlockSize;

    while (fruLength > 0)
    {
        size_t requestLength =
            std::min(static_cast<size_t>(I2C_SMBUS_BLOCK_MAX), fruLength);

        if (readBlock(flag, file, address, static_cast<uint16_t>(readOffset),
                      static_cast<uint8_t>(requestLength),
                      blockData.data()) < 0)
        {
            std::cerr << "failed to read " << errorHelp << "\n";
            return {};
        }

        device.insert(device.end(), blockData.begin(),
                      blockData.begin() + requestLength);

        readOffset += requestLength;
        fruLength -= std::min(requestLength, fruLength);
    }

    return device;
}

unsigned int getHeaderAreaFieldOffset(fruAreas area)
{
    return static_cast<unsigned int>(area) + 1;
}
