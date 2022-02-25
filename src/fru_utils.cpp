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
/// \file fru_utils.cpp

#include "fru_utils.hpp"
#include "fru_device.hpp"

#include "utils.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <set>
#include <string>
#include <vector>

extern "C"
{
// Include for I2C_SMBUS_BLOCK_MAX
#include <linux/i2c.h>
}

static constexpr bool debug = false;
constexpr size_t fruVersion = 1; // Current FRU spec version number is 1

std::tm intelEpoch(void)
{
    std::tm val = {};
    val.tm_year = 1996 - 1900;
    val.tm_mday = 1;
    return val;
}

char sixBitToChar(uint8_t val)
{
    return static_cast<char>((val & 0x3f) + ' ');
}

char bcdPlusToChar(uint8_t val)
{
    val &= 0xf;
    return (val < 10) ? static_cast<char>(val + '0') : bcdHighChars[val - 10];
}

enum FRUDataEncoding
{
    binary = 0x0,
    bcdPlus = 0x1,
    sixBitASCII = 0x2,
    languageDependent = 0x3,
};

bool isMuxBus(size_t bus)
{
    return is_symlink(std::filesystem::path(
        "/sys/bus/i2c/devices/i2c-" + std::to_string(bus) + "/mux_device"));
}

/* Decode FRU data into a std::string, given an input iterator and end. If the
 * state returned is fruDataOk, then the resulting string is the decoded FRU
 * data. The input iterator is advanced past the data consumed.
 *
 * On fruDataErr, we have lost synchronisation with the length bytes, so the
 * iterator is no longer usable.
 */
std::pair<DecodeState, std::string>
    decodeFRUData(std::vector<uint8_t>::const_iterator& iter,
                  const std::vector<uint8_t>::const_iterator& end,
                  bool isLangEng)
{
    std::string value;
    unsigned int i;

    /* we need at least one byte to decode the type/len header */
    if (iter == end)
    {
        std::cerr << "Truncated FRU data\n";
        return make_pair(DecodeState::err, value);
    }

    uint8_t c = *(iter++);

    /* 0xc1 is the end marker */
    if (c == 0xc1)
    {
        return make_pair(DecodeState::end, value);
    }

    /* decode type/len byte */
    uint8_t type = static_cast<uint8_t>(c >> 6);
    uint8_t len = static_cast<uint8_t>(c & 0x3f);

    /* we should have at least len bytes of data available overall */
    if (iter + len > end)
    {
        std::cerr << "FRU data field extends past end of FRU area data\n";
        return make_pair(DecodeState::err, value);
    }

    switch (type)
    {
        case FRUDataEncoding::binary:
        {
            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            for (i = 0; i < len; i++, iter++)
            {
                uint8_t val = static_cast<uint8_t>(*iter);
                ss << std::setw(2) << static_cast<int>(val);
            }
            value = ss.str();
            break;
        }
        case FRUDataEncoding::languageDependent:
            /* For language-code dependent encodings, assume 8-bit ASCII */
            value = std::string(iter, iter + len);
            iter += len;

            /* English text is encoded in 8-bit ASCII + Latin 1. All other
             * languages are required to use 2-byte unicode. FruDevice does not
             * handle unicode.
             */
            if (!isLangEng)
            {
                std::cerr << "Error: Non english string is not supported \n";
                return make_pair(DecodeState::err, value);
            }

            break;

        case FRUDataEncoding::bcdPlus:
            value = std::string();
            for (i = 0; i < len; i++, iter++)
            {
                uint8_t val = *iter;
                value.push_back(bcdPlusToChar(val >> 4));
                value.push_back(bcdPlusToChar(val & 0xf));
            }
            break;

        case FRUDataEncoding::sixBitASCII:
        {
            unsigned int accum = 0;
            unsigned int accumBitLen = 0;
            value = std::string();
            for (i = 0; i < len; i++, iter++)
            {
                accum |= *iter << accumBitLen;
                accumBitLen += 8;
                while (accumBitLen >= 6)
                {
                    value.push_back(sixBitToChar(accum & 0x3f));
                    accum >>= 6;
                    accumBitLen -= 6;
                }
            }
        }
        break;
    }

    return make_pair(DecodeState::ok, value);
}

bool checkLangEng(uint8_t lang)
{
    // If Lang is not English then the encoding is defined as 2-byte UNICODE,
    // but we don't support that.
    if (lang && lang != 25)
    {
        std::cerr << "Warning: languages other than English is not "
                     "supported\n";
        // Return language flag as non english
        return false;
    }
    return true;
}

/* This function verifies for other offsets to check if they are not
 * falling under other field area
 *
 * fruBytes:    Start of Fru data
 * currentArea: Index of current area offset to be compared against all area
 *              offset and it is a multiple of 8 bytes as per specification
 * len:         Length of current area space and it is a multiple of 8 bytes
 *              as per specification
 */
bool verifyOffset(const std::vector<uint8_t>& fruBytes, fruAreas currentArea,
                  uint8_t len)
{

    unsigned int fruBytesSize = fruBytes.size();

    // check if Fru data has at least 8 byte header
    if (fruBytesSize <= fruBlockSize)
    {
        std::cerr << "Error: trying to parse empty FRU\n";
        return false;
    }

    // Check range of passed currentArea value
    if (currentArea > fruAreas::fruAreaMultirecord)
    {
        std::cerr << "Error: Fru area is out of range\n";
        return false;
    }

    unsigned int currentAreaIndex = getHeaderAreaFieldOffset(currentArea);
    if (currentAreaIndex > fruBytesSize)
    {
        std::cerr << "Error: Fru area index is out of range\n";
        return false;
    }

    unsigned int start = fruBytes[currentAreaIndex];
    unsigned int end = start + len;

    /* Verify each offset within the range of start and end */
    for (fruAreas area = fruAreas::fruAreaInternal;
         area <= fruAreas::fruAreaMultirecord; ++area)
    {
        // skip the current offset
        if (area == currentArea)
        {
            continue;
        }

        unsigned int areaIndex = getHeaderAreaFieldOffset(area);
        if (areaIndex > fruBytesSize)
        {
            std::cerr << "Error: Fru area index is out of range\n";
            return false;
        }

        unsigned int areaOffset = fruBytes[areaIndex];
        // if areaOffset is 0 means this area is not available so skip
        if (areaOffset == 0)
        {
            continue;
        }

        // check for overlapping of current offset with given areaoffset
        if (areaOffset == start || (areaOffset > start && areaOffset < end))
        {
            std::cerr << getFruAreaName(currentArea)
                      << " offset is overlapping with " << getFruAreaName(area)
                      << " offset\n";
            return false;
        }
    }
    return true;
}

resCodes
    formatIPMIFRU(const std::vector<uint8_t>& fruBytes,
                  boost::container::flat_map<std::string, std::string>& result)
{
    resCodes ret = resCodes::resOK;
    if (fruBytes.size() <= fruBlockSize)
    {
        std::cerr << "Error: trying to parse empty FRU \n";
        return resCodes::resErr;
    }
    result["Common_Format_Version"] =
        std::to_string(static_cast<int>(*fruBytes.begin()));

    const std::vector<std::string>* fruAreaFieldNames;

    // Don't parse Internal and Multirecord areas
    for (fruAreas area = fruAreas::fruAreaChassis;
         area <= fruAreas::fruAreaProduct; ++area)
    {

        size_t offset = *(fruBytes.begin() + getHeaderAreaFieldOffset(area));
        if (offset == 0)
        {
            continue;
        }
        offset *= fruBlockSize;
        std::vector<uint8_t>::const_iterator fruBytesIter =
            fruBytes.begin() + offset;
        if (fruBytesIter + fruBlockSize >= fruBytes.end())
        {
            std::cerr << "Not enough data to parse \n";
            return resCodes::resErr;
        }
        // check for format version 1
        if (*fruBytesIter != 0x01)
        {
            std::cerr << "Unexpected version " << *fruBytesIter << "\n";
            return resCodes::resErr;
        }
        ++fruBytesIter;

        /* Verify other area offset for overlap with current area by passing
         * length of current area offset pointed by *fruBytesIter
         */
        if (!verifyOffset(fruBytes, area, *fruBytesIter))
        {
            return resCodes::resErr;
        }

        size_t fruAreaSize = *fruBytesIter * fruBlockSize;
        std::vector<uint8_t>::const_iterator fruBytesIterEndArea =
            fruBytes.begin() + offset + fruAreaSize - 1;
        ++fruBytesIter;

        uint8_t fruComputedChecksum =
            calculateChecksum(fruBytes.begin() + offset, fruBytesIterEndArea);
        if (fruComputedChecksum != *fruBytesIterEndArea)
        {
            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            ss << "Checksum error in FRU area " << getFruAreaName(area) << "\n";
            ss << "\tComputed checksum: 0x" << std::setw(2)
               << static_cast<int>(fruComputedChecksum) << "\n";
            ss << "\tThe read checksum: 0x" << std::setw(2)
               << static_cast<int>(*fruBytesIterEndArea) << "\n";
            std::cerr << ss.str();
            ret = resCodes::resWarn;
        }

        /* Set default language flag to true as Chassis Fru area are always
         * encoded in English defined in Section 10 of Fru specification
         */

        bool isLangEng = true;
        switch (area)
        {
            case fruAreas::fruAreaChassis:
            {
                result["CHASSIS_TYPE"] =
                    std::to_string(static_cast<int>(*fruBytesIter));
                fruBytesIter += 1;
                fruAreaFieldNames = &chassisFruAreas;
                break;
            }
            case fruAreas::fruAreaBoard:
            {
                uint8_t lang = *fruBytesIter;
                result["BOARD_LANGUAGE_CODE"] =
                    std::to_string(static_cast<int>(lang));
                isLangEng = checkLangEng(lang);
                fruBytesIter += 1;

                unsigned int minutes = *fruBytesIter |
                                       *(fruBytesIter + 1) << 8 |
                                       *(fruBytesIter + 2) << 16;
                std::tm fruTime = intelEpoch();
                std::time_t timeValue = std::mktime(&fruTime);
                timeValue += minutes * 60;
                fruTime = *std::gmtime(&timeValue);

                // Tue Nov 20 23:08:00 2018
                char timeString[32] = {0};
                auto bytes = std::strftime(timeString, sizeof(timeString),
                                           "%Y-%m-%d - %H:%M:%S", &fruTime);
                if (bytes == 0)
                {
                    std::cerr << "invalid time string encountered\n";
                    return resCodes::resErr;
                }

                result["BOARD_MANUFACTURE_DATE"] = std::string(timeString);
                fruBytesIter += 3;
                fruAreaFieldNames = &boardFruAreas;
                break;
            }
            case fruAreas::fruAreaProduct:
            {
                uint8_t lang = *fruBytesIter;
                result["PRODUCT_LANGUAGE_CODE"] =
                    std::to_string(static_cast<int>(lang));
                isLangEng = checkLangEng(lang);
                fruBytesIter += 1;
                fruAreaFieldNames = &productFruAreas;
                break;
            }
            default:
            {
                std::cerr << "Internal error: unexpected FRU area index: "
                          << static_cast<int>(area) << " \n";
                return resCodes::resErr;
            }
        }
        size_t fieldIndex = 0;
        DecodeState state;
        do
        {
            auto res =
                decodeFRUData(fruBytesIter, fruBytesIterEndArea, isLangEng);
            state = res.first;
            std::string value = res.second;
            std::string name;
            if (fieldIndex < fruAreaFieldNames->size())
            {
                name = std::string(getFruAreaName(area)) + "_" +
                       fruAreaFieldNames->at(fieldIndex);
            }
            else
            {
                name =
                    std::string(getFruAreaName(area)) + "_" +
                    fruCustomFieldName +
                    std::to_string(fieldIndex - fruAreaFieldNames->size() + 1);
            }

            if (state == DecodeState::ok)
            {
                // Strip non null characters from the end
                value.erase(std::find_if(value.rbegin(), value.rend(),
                                         [](char ch) { return ch != 0; })
                                .base(),
                            value.end());

                result[name] = std::move(value);
                ++fieldIndex;
            }
            else if (state == DecodeState::err)
            {
                std::cerr << "Error while parsing " << name << "\n";
                ret = resCodes::resWarn;
                // Cancel decoding if failed to parse any of mandatory
                // fields
                if (fieldIndex < fruAreaFieldNames->size())
                {
                    std::cerr << "Failed to parse mandatory field \n";
                    return resCodes::resErr;
                }
            }
            else
            {
                if (fieldIndex < fruAreaFieldNames->size())
                {
                    std::cerr << "Mandatory fields absent in FRU area "
                              << getFruAreaName(area) << " after " << name
                              << "\n";
                    ret = resCodes::resWarn;
                }
            }
        } while (state == DecodeState::ok);
        for (; fruBytesIter < fruBytesIterEndArea; fruBytesIter++)
        {
            uint8_t c = *fruBytesIter;
            if (c)
            {
                std::cerr << "Non-zero byte after EndOfFields in FRU area "
                          << getFruAreaName(area) << "\n";
                ret = resCodes::resWarn;
                break;
            }
        }
    }

    return ret;
}

// Calculate new checksum for fru info area
uint8_t calculateChecksum(std::vector<uint8_t>::const_iterator iter,
                          std::vector<uint8_t>::const_iterator end)
{
    constexpr int checksumMod = 256;
    uint8_t sum = std::accumulate(iter, end, static_cast<uint8_t>(0));
    return (checksumMod - sum) % checksumMod;
}

uint8_t calculateChecksum(std::vector<uint8_t>& fruAreaData)
{
    return calculateChecksum(fruAreaData.begin(), fruAreaData.end());
}

// Update new fru area length &
// Update checksum at new checksum location
// Return the offset of the area checksum byte
unsigned int updateFRUAreaLenAndChecksum(std::vector<uint8_t>& fruData,
                                         size_t fruAreaStart,
                                         size_t fruAreaEndOfFieldsOffset,
                                         size_t fruAreaEndOffset)
{
    size_t traverseFRUAreaIndex = fruAreaEndOfFieldsOffset - fruAreaStart;

    // fill zeros for any remaining unused space
    std::fill(fruData.begin() + fruAreaEndOfFieldsOffset,
              fruData.begin() + fruAreaEndOffset, 0);

    size_t mod = traverseFRUAreaIndex % fruBlockSize;
    size_t checksumLoc;
    if (!mod)
    {
        traverseFRUAreaIndex += (fruBlockSize);
        checksumLoc = fruAreaEndOfFieldsOffset + (fruBlockSize - 1);
    }
    else
    {
        traverseFRUAreaIndex += (fruBlockSize - mod);
        checksumLoc = fruAreaEndOfFieldsOffset + (fruBlockSize - mod - 1);
    }

    size_t newFRUAreaLen = (traverseFRUAreaIndex / fruBlockSize) +
                           ((traverseFRUAreaIndex % fruBlockSize) != 0);
    size_t fruAreaLengthLoc = fruAreaStart + 1;
    fruData[fruAreaLengthLoc] = static_cast<uint8_t>(newFRUAreaLen);

    // Calculate new checksum
    std::vector<uint8_t> finalFRUData;
    std::copy_n(fruData.begin() + fruAreaStart, checksumLoc - fruAreaStart,
                std::back_inserter(finalFRUData));

    fruData[checksumLoc] = calculateChecksum(finalFRUData);
    return checksumLoc;
}

ssize_t getFieldLength(uint8_t fruFieldTypeLenValue)
{
    constexpr uint8_t typeLenMask = 0x3F;
    constexpr uint8_t endOfFields = 0xC1;
    if (fruFieldTypeLenValue == endOfFields)
    {
        return -1;
    }
    return fruFieldTypeLenValue & typeLenMask;
}

bool validateHeader(const std::array<uint8_t, I2C_SMBUS_BLOCK_MAX>& blockData)
{
    // ipmi spec format version number is currently at 1, verify it
    if (blockData[0] != fruVersion)
    {
        if (debug)
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
        if (debug)
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
        if (debug)
        {
            std::cerr << "Checksum " << (int)(blockData[7])
                      << " is invalid. calculated checksum is " << (int)(sum)
                      << "\n";
        }
        return false;
    }
    return true;
}

bool findFRUHeader(int flag, int file, uint16_t address,
                   const ReadBlockFunc& readBlock, const std::string& errorHelp,
                   std::array<uint8_t, I2C_SMBUS_BLOCK_MAX>& blockData,
                   off_t& baseOffset)
{
    if (readBlock(flag, file, address, baseOffset, 0x8, blockData.data()) < 0)
    {
        std::cerr << "failed to read " << errorHelp << " base offset "
                  << baseOffset << "\n";
        return false;
    }

    // check the header checksum
    if (validateHeader(blockData))
    {
        return true;
    }

    // only continue the search if we just looked at 0x0.
    if (baseOffset != 0)
    {
        return false;
    }

    // now check for special cases where the IPMI data is at an offset

    // check if blockData starts with tyanHeader
    const std::vector<uint8_t> tyanHeader = {'$', 'T', 'Y', 'A', 'N', '$'};
    if (blockData.size() >= tyanHeader.size() &&
        std::equal(tyanHeader.begin(), tyanHeader.end(), blockData.begin()))
    {
        // look for the FRU header at offset 0x6000
        baseOffset = 0x6000;
        return findFRUHeader(flag, file, address, readBlock, errorHelp,
                             blockData, baseOffset);
    }

    if (debug)
    {
        std::cerr << "Illegal header " << errorHelp << " base offset "
                  << baseOffset << "\n";
    }

    return false;
}

std::vector<uint8_t> readFRUContents(int flag, int file, uint16_t address,
                                     const ReadBlockFunc& readBlock,
                                     const std::string& errorHelp)
{
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;
    off_t baseOffset = 0x0;

    if (!findFRUHeader(flag, file, address, readBlock, errorHelp, blockData,
                       baseOffset))
    {
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

        if (readBlock(flag, file, address, baseOffset + areaOffset, 0x2,
                      blockData.data()) < 0)
        {
            std::cerr << "failed to read " << errorHelp << " base offset "
                      << baseOffset << "\n";
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
            if (readBlock(flag, file, address, baseOffset + areaOffset, 0x3,
                          blockData.data()) < 0)
            {
                std::cerr << "failed to read " << errorHelp << " base offset "
                          << baseOffset << "\n";
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

        if (readBlock(flag, file, address, baseOffset + readOffset,
                      requestLength, blockData.data()) < 0)
        {
            std::cerr << "failed to read " << errorHelp << " base offset "
                      << baseOffset << "\n";
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

std::vector<uint8_t>& getFRUInfo(const uint8_t& bus, const uint8_t& address)
{
    auto deviceMap = busMap.find(bus);
    if (deviceMap == busMap.end())
    {
        throw std::invalid_argument("Invalid Bus.");
    }
    auto device = deviceMap->second->find(address);
    if (device == deviceMap->second->end())
    {
        throw std::invalid_argument("Invalid Address.");
    }
    std::vector<uint8_t>& ret = device->second;

    return ret;
}

// Details with example of Asset Tag Update
// To find location of Product Info Area asset tag as per FRU specification
// 1. Find product Info area starting offset (*8 - as header will be in
// multiple of 8 bytes).
// 2. Skip 3 bytes of product info area (like format version, area length,
// and language code).
// 3. Traverse manufacturer name, product name, product version, & product
// serial number, by reading type/length code to reach the Asset Tag.
// 4. Update the Asset Tag, reposition the product Info area in multiple of
// 8 bytes. Update the Product area length and checksum.

bool updateFRUProperty(
    const std::string& updatePropertyReq, uint32_t bus, uint32_t address,
    const std::string& propertyName,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    size_t& unknownBusObjectCount, const bool& powerIsOn,
    sdbusplus::asio::object_server& objServer,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus)
{
    size_t updatePropertyReqLen = updatePropertyReq.length();
    if (updatePropertyReqLen == 1 || updatePropertyReqLen > 63)
    {
        std::cerr
            << "FRU field data cannot be of 1 char or more than 63 chars. "
               "Invalid Length "
            << updatePropertyReqLen << "\n";
        return false;
    }

    std::vector<uint8_t> fruData;
    try
    {
        fruData = getFRUInfo(static_cast<uint8_t>(bus),
                             static_cast<uint8_t>(address));
    }
    catch (const std::invalid_argument& e)
    {
        std::cerr << "Failure getting FRU Info" << e.what() << "\n";
        return false;
    }

    if (fruData.empty())
    {
        return false;
    }

    const std::vector<std::string>* fruAreaFieldNames;

    uint8_t fruAreaOffsetFieldValue = 0;
    size_t offset = 0;
    std::string areaName = propertyName.substr(0, propertyName.find('_'));
    std::string propertyNamePrefix = areaName + "_";
    auto it = std::find(fruAreaNames.begin(), fruAreaNames.end(), areaName);
    if (it == fruAreaNames.end())
    {
        std::cerr << "Can't parse area name for property " << propertyName
                  << " \n";
        return false;
    }
    fruAreas fruAreaToUpdate = static_cast<fruAreas>(it - fruAreaNames.begin());
    fruAreaOffsetFieldValue =
        fruData[getHeaderAreaFieldOffset(fruAreaToUpdate)];
    switch (fruAreaToUpdate)
    {
        case fruAreas::fruAreaChassis:
            offset = 3; // chassis part number offset. Skip fixed first 3 bytes
            fruAreaFieldNames = &chassisFruAreas;
            break;
        case fruAreas::fruAreaBoard:
            offset = 6; // board manufacturer offset. Skip fixed first 6 bytes
            fruAreaFieldNames = &boardFruAreas;
            break;
        case fruAreas::fruAreaProduct:
            // Manufacturer name offset. Skip fixed first 3 product fru bytes
            // i.e. version, area length and language code
            offset = 3;
            fruAreaFieldNames = &productFruAreas;
            break;
        default:
            std::cerr << "Don't know how to handle property " << propertyName
                      << " \n";
            return false;
    }
    if (fruAreaOffsetFieldValue == 0)
    {
        std::cerr << "FRU Area for " << propertyName << " not present \n";
        return false;
    }

    size_t fruAreaStart = fruAreaOffsetFieldValue * fruBlockSize;
    size_t fruAreaSize = fruData[fruAreaStart + 1] * fruBlockSize;
    size_t fruAreaEnd = fruAreaStart + fruAreaSize;
    size_t fruDataIter = fruAreaStart + offset;
    size_t skipToFRUUpdateField = 0;
    ssize_t fieldLength;

    bool found = false;
    for (auto& field : *fruAreaFieldNames)
    {
        skipToFRUUpdateField++;
        if (propertyName == propertyNamePrefix + field)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        std::size_t pos = propertyName.find(fruCustomFieldName);
        if (pos == std::string::npos)
        {
            std::cerr << "PropertyName doesn't exist in FRU Area Vectors: "
                      << propertyName << "\n";
            return false;
        }
        std::string fieldNumStr =
            propertyName.substr(pos + fruCustomFieldName.length());
        size_t fieldNum = std::stoi(fieldNumStr);
        if (fieldNum == 0)
        {
            std::cerr << "PropertyName not recognized: " << propertyName
                      << "\n";
            return false;
        }
        skipToFRUUpdateField += fieldNum;
    }

    for (size_t i = 1; i < skipToFRUUpdateField; i++)
    {
        fieldLength = getFieldLength(fruData[fruDataIter]);
        if (fieldLength < 0)
        {
            break;
        }
        fruDataIter += 1 + fieldLength;
    }
    size_t fruUpdateFieldLoc = fruDataIter;

    // Push post update fru field bytes to a vector
    fieldLength = getFieldLength(fruData[fruUpdateFieldLoc]);
    if (fieldLength < 0)
    {
        std::cerr << "Property " << propertyName << " not present \n";
        return false;
    }
    fruDataIter += 1 + fieldLength;
    size_t restFRUFieldsLoc = fruDataIter;
    size_t endOfFieldsLoc = 0;
    while ((fieldLength = getFieldLength(fruData[fruDataIter])) >= 0)
    {
        if (fruDataIter >= (fruAreaStart + fruAreaSize))
        {
            fruDataIter = fruAreaStart + fruAreaSize;
            break;
        }
        fruDataIter += 1 + fieldLength;
    }
    endOfFieldsLoc = fruDataIter;

    std::vector<uint8_t> restFRUAreaFieldsData;
    std::copy_n(fruData.begin() + restFRUFieldsLoc,
                endOfFieldsLoc - restFRUFieldsLoc + 1,
                std::back_inserter(restFRUAreaFieldsData));

    // Push post update fru areas if any
    unsigned int nextFRUAreaLoc = 0;
    for (fruAreas nextFRUArea = fruAreas::fruAreaInternal;
         nextFRUArea <= fruAreas::fruAreaMultirecord; ++nextFRUArea)
    {
        unsigned int fruAreaLoc =
            fruData[getHeaderAreaFieldOffset(nextFRUArea)] * fruBlockSize;
        if ((fruAreaLoc > endOfFieldsLoc) &&
            ((nextFRUAreaLoc == 0) || (fruAreaLoc < nextFRUAreaLoc)))
        {
            nextFRUAreaLoc = fruAreaLoc;
        }
    }
    std::vector<uint8_t> restFRUAreasData;
    if (nextFRUAreaLoc)
    {
        std::copy_n(fruData.begin() + nextFRUAreaLoc,
                    fruData.size() - nextFRUAreaLoc,
                    std::back_inserter(restFRUAreasData));
    }

    // check FRU area size
    size_t fruAreaDataSize =
        ((fruUpdateFieldLoc - fruAreaStart + 1) + restFRUAreaFieldsData.size());
    size_t fruAreaAvailableSize = fruAreaSize - fruAreaDataSize;
    if ((updatePropertyReqLen + 1) > fruAreaAvailableSize)
    {

#ifdef ENABLE_FRU_AREA_RESIZE
        size_t newFRUAreaSize = fruAreaDataSize + updatePropertyReqLen + 1;
        // round size to 8-byte blocks
        newFRUAreaSize =
            ((newFRUAreaSize - 1) / fruBlockSize + 1) * fruBlockSize;
        size_t newFRUDataSize = fruData.size() + newFRUAreaSize - fruAreaSize;
        fruData.resize(newFRUDataSize);
        fruAreaSize = newFRUAreaSize;
        fruAreaEnd = fruAreaStart + fruAreaSize;
#else
        std::cerr << "FRU field length: " << updatePropertyReqLen + 1
                  << " should not be greater than available FRU area size: "
                  << fruAreaAvailableSize << "\n";
        return false;
#endif // ENABLE_FRU_AREA_RESIZE
    }

    // write new requested property field length and data
    constexpr uint8_t newTypeLenMask = 0xC0;
    fruData[fruUpdateFieldLoc] =
        static_cast<uint8_t>(updatePropertyReqLen | newTypeLenMask);
    fruUpdateFieldLoc++;
    std::copy(updatePropertyReq.begin(), updatePropertyReq.end(),
              fruData.begin() + fruUpdateFieldLoc);

    // Copy remaining data to main fru area - post updated fru field vector
    restFRUFieldsLoc = fruUpdateFieldLoc + updatePropertyReqLen;
    size_t fruAreaDataEnd = restFRUFieldsLoc + restFRUAreaFieldsData.size();
    std::copy(restFRUAreaFieldsData.begin(), restFRUAreaFieldsData.end(),
              fruData.begin() + restFRUFieldsLoc);

    // Update final fru with new fru area length and checksum
    unsigned int nextFRUAreaNewLoc = updateFRUAreaLenAndChecksum(
        fruData, fruAreaStart, fruAreaDataEnd, fruAreaEnd);

#ifdef ENABLE_FRU_AREA_RESIZE
    ++nextFRUAreaNewLoc;
    ssize_t nextFRUAreaOffsetDiff =
        (nextFRUAreaNewLoc - nextFRUAreaLoc) / fruBlockSize;
    // Append rest FRU Areas if size changed and there were other sections after
    // updated one
    if (nextFRUAreaOffsetDiff && nextFRUAreaLoc)
    {
        std::copy(restFRUAreasData.begin(), restFRUAreasData.end(),
                  fruData.begin() + nextFRUAreaNewLoc);
        // Update Common Header
        for (int fruArea = fruAreaInternal; fruArea <= fruAreaMultirecord;
             fruArea++)
        {
            unsigned int fruAreaOffsetField = getHeaderAreaFieldOffset(fruArea);
            size_t curFRUAreaOffset = fruData[fruAreaOffsetField];
            if (curFRUAreaOffset > fruAreaOffsetFieldValue)
            {
                fruData[fruAreaOffsetField] = static_cast<int8_t>(
                    curFRUAreaOffset + nextFRUAreaOffsetDiff);
            }
        }
        // Calculate new checksum
        std::vector<uint8_t> headerFRUData;
        std::copy_n(fruData.begin(), 7, std::back_inserter(headerFRUData));
        size_t checksumVal = calculateChecksum(headerFRUData);
        fruData[7] = static_cast<uint8_t>(checksumVal);
        // fill zeros if FRU Area size decreased
        if (nextFRUAreaOffsetDiff < 0)
        {
            std::fill(fruData.begin() + nextFRUAreaNewLoc +
                          restFRUAreasData.size(),
                      fruData.end(), 0);
        }
    }
#else
    // this is to avoid "unused variable" warning
    (void)nextFRUAreaNewLoc;
#endif // ENABLE_FRU_AREA_RESIZE
    if (fruData.empty())
    {
        return false;
    }

    if (!writeFRU(static_cast<uint8_t>(bus), static_cast<uint8_t>(address),
                  fruData))
    {
        return false;
    }

    // Rescan the bus so that GetRawFru dbus-call fetches updated values
    rescanBusses(busMap, dbusInterfaceMap, unknownBusObjectCount, powerIsOn,
                 objServer, systemBus);
    return true;
}

void addFruObjectToDbus(
    std::vector<uint8_t>& device,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    uint32_t bus, uint32_t address, size_t& unknownBusObjectCount,
    const bool& powerIsOn, sdbusplus::asio::object_server& objServer,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus)
{
    boost::container::flat_map<std::string, std::string> formattedFRU;
    resCodes res = formatIPMIFRU(device, formattedFRU);
    if (res == resCodes::resErr)
    {
        std::cerr << "failed to parse FRU for device at bus " << bus
                  << " address " << address << "\n";
        return;
    }
    if (res == resCodes::resWarn)
    {
        std::cerr << "there were warnings while parsing FRU for device at bus "
                  << bus << " address " << address << "\n";
    }

    auto productNameFind = formattedFRU.find("BOARD_PRODUCT_NAME");
    std::string productName;
    // Not found under Board section or an empty string.
    if (productNameFind == formattedFRU.end() ||
        productNameFind->second.empty())
    {
        productNameFind = formattedFRU.find("PRODUCT_PRODUCT_NAME");
    }
    // Found under Product section and not an empty string.
    if (productNameFind != formattedFRU.end() &&
        !productNameFind->second.empty())
    {
        productName = productNameFind->second;
        std::regex illegalObject("[^A-Za-z0-9_]");
        productName = std::regex_replace(productName, illegalObject, "_");
    }
    else
    {
        productName = "UNKNOWN" + std::to_string(unknownBusObjectCount);
        unknownBusObjectCount++;
    }

    productName = "/xyz/openbmc_project/FruDevice/" + productName;
    // avoid duplicates by checking to see if on a mux
    if (bus > 0)
    {
        int highest = -1;
        bool found = false;

        for (auto const& busIface : dbusInterfaceMap)
        {
            std::string path = busIface.second->get_object_path();
            if (std::regex_match(path, std::regex(productName + "(_\\d+|)$")))
            {
                if (isMuxBus(bus) && address == busIface.first.second &&
                    (getFRUInfo(static_cast<uint8_t>(busIface.first.first),
                                static_cast<uint8_t>(busIface.first.second)) ==
                     getFRUInfo(static_cast<uint8_t>(bus),
                                static_cast<uint8_t>(address))))
                {
                    // This device is already added to the lower numbered bus,
                    // do not replicate it.
                    return;
                }

                // Check if the match named has extra information.
                found = true;
                std::smatch baseMatch;

                bool match = std::regex_match(
                    path, baseMatch, std::regex(productName + "_(\\d+)$"));
                if (match)
                {
                    if (baseMatch.size() == 2)
                    {
                        std::ssub_match baseSubMatch = baseMatch[1];
                        std::string base = baseSubMatch.str();

                        int value = std::stoi(base);
                        highest = (value > highest) ? value : highest;
                    }
                }
            }
        } // end searching objects

        if (found)
        {
            // We found something with the same name.  If highest was still -1,
            // it means this new entry will be _0.
            productName += "_";
            productName += std::to_string(++highest);
        }
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface(productName, "xyz.openbmc_project.FruDevice");
    dbusInterfaceMap[std::pair<size_t, size_t>(bus, address)] = iface;

    for (auto& property : formattedFRU)
    {

        std::regex_replace(property.second.begin(), property.second.begin(),
                           property.second.end(), nonAsciiRegex, "_");
        if (property.second.empty() && property.first != "PRODUCT_ASSET_TAG")
        {
            continue;
        }
        std::string key =
            std::regex_replace(property.first, nonAsciiRegex, "_");

        if (property.first == "PRODUCT_ASSET_TAG")
        {
            std::string propertyName = property.first;
            iface->register_property(
                key, property.second + '\0',
                [bus, address, propertyName, &dbusInterfaceMap,
                 &unknownBusObjectCount, &powerIsOn, &objServer,
                 &systemBus](const std::string& req, std::string& resp) {
                    if (strcmp(req.c_str(), resp.c_str()) != 0)
                    {
                        // call the method which will update
                        if (updateFRUProperty(req, bus, address, propertyName,
                                              dbusInterfaceMap,
                                              unknownBusObjectCount, powerIsOn,
                                              objServer, systemBus))
                        {
                            resp = req;
                        }
                        else
                        {
                            throw std::invalid_argument(
                                "FRU property update failed.");
                        }
                    }
                    return 1;
                });
        }
        else if (!iface->register_property(key, property.second + '\0'))
        {
            std::cerr << "illegal key: " << key << "\n";
        }
        if (debug)
        {
            std::cout << property.first << ": " << property.second << "\n";
        }
    }

    // baseboard will be 0, 0
    iface->register_property("BUS", bus);
    iface->register_property("ADDRESS", address);

    iface->initialize();
}
