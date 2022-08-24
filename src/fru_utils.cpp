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

#include <array>
#include <cstddef>
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

bool isMuxBus(size_t bus)
{
    return is_symlink(std::filesystem::path(
        "/sys/bus/i2c/devices/i2c-" + std::to_string(bus) + "/mux_device"));
}

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
    unsigned int i = 0;

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
    if ((lang != 0U) && lang != 25)
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

    const std::vector<std::string>* fruAreaFieldNames = nullptr;

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
                timeValue += static_cast<long>(minutes) * 60;
                fruTime = *std::gmtime(&timeValue);

                // Tue Nov 20 23:08:00 2018
                std::array<char, 32> timeString = {};
                auto bytes = std::strftime(timeString.data(), timeString.size(),
                                           "%Y-%m-%d - %H:%M:%S", &fruTime);
                if (bytes == 0)
                {
                    std::cerr << "invalid time string encountered\n";
                    return resCodes::resErr;
                }

                result["BOARD_MANUFACTURE_DATE"] =
                    std::string_view(timeString.data(), bytes);
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
        DecodeState state = DecodeState::ok;
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
            if (c != 0U)
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
    size_t checksumLoc = 0;
    if (mod == 0U)
    {
        traverseFRUAreaIndex += (fruBlockSize);
        checksumLoc = fruAreaEndOfFieldsOffset + (fruBlockSize - 1);
    }
    else
    {
        traverseFRUAreaIndex += (fruBlockSize - mod);
        checksumLoc = fruAreaEndOfFieldsOffset + (fruBlockSize - mod - 1);
    }

    size_t newFRUAreaLen =
        (traverseFRUAreaIndex / fruBlockSize) +
        static_cast<unsigned long>((traverseFRUAreaIndex % fruBlockSize) != 0);
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

bool findFRUHeader(FRUReader& reader, const std::string& errorHelp,
                   std::array<uint8_t, I2C_SMBUS_BLOCK_MAX>& blockData,
                   off_t& baseOffset)
{
    if (reader.read(baseOffset, 0x8, blockData.data()) < 0)
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
        return findFRUHeader(reader, errorHelp, blockData, baseOffset);
    }

    if (debug)
    {
        std::cerr << "Illegal header " << errorHelp << " base offset "
                  << baseOffset << "\n";
    }

    return false;
}

std::vector<uint8_t> readFRUContents(FRUReader& reader,
                                     const std::string& errorHelp)
{
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData{};
    off_t baseOffset = 0x0;

    if (!findFRUHeader(reader, errorHelp, blockData, baseOffset))
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

        if (reader.read(baseOffset + areaOffset, 0x2, blockData.data()) < 0)
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
            if (reader.read(baseOffset + areaOffset, 0x3, blockData.data()) < 0)
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
            if ((blockData[1] & multiRecordEndOfListMask) != 0)
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

        if (reader.read(baseOffset + readOffset, requestLength,
                        blockData.data()) < 0)
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

void searchFRUDbusObjects(
    uint32_t bus, uint32_t address,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    std::string& productName)
{

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
                if (isMuxBus(bus) && bus != busIface.first.first &&
                    address == busIface.first.second &&
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
}
