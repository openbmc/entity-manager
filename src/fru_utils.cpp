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

#include <phosphor-logging/lg2.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>

extern "C"
{
// Include for I2C_SMBUS_BLOCK_MAX
#include <linux/i2c.h>
}

constexpr size_t fruVersion = 1; // Current FRU spec version number is 1

std::tm intelEpoch()
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

enum MultiRecordType : uint8_t
{
    powerSupplyInfo = 0x00,
    dcOutput = 0x01,
    dcLoad = 0x02,
    managementAccessRecord = 0x03,
    baseCompatibilityRecord = 0x04,
    extendedCompatibilityRecord = 0x05,
    resvASFSMBusDeviceRecord = 0x06,
    resvASFLegacyDeviceAlerts = 0x07,
    resvASFRemoteControl = 0x08,
    extendedDCOutput = 0x09,
    extendedDCLoad = 0x0A
};

enum SubManagementAccessRecord : uint8_t
{
    systemManagementURL = 0x01,
    systemName = 0x02,
    systemPingAddress = 0x03,
    componentManagementURL = 0x04,
    componentName = 0x05,
    componentPingAddress = 0x06,
    systemUniqueID = 0x07
};

/* Decode FRU data into a std::string, given an input iterator and end. If the
 * state returned is fruDataOk, then the resulting string is the decoded FRU
 * data. The input iterator is advanced past the data consumed.
 *
 * On fruDataErr, we have lost synchronisation with the length bytes, so the
 * iterator is no longer usable.
 */
std::pair<DecodeState, std::string> decodeFRUData(
    std::vector<uint8_t>::const_iterator& iter,
    const std::vector<uint8_t>::const_iterator& end, bool isLangEng)
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

        default:
        {
            return make_pair(DecodeState::err, value);
        }
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

static void parseMultirecordUUID(
    const std::vector<uint8_t>& device,
    boost::container::flat_map<std::string, std::string>& result)
{
    constexpr size_t uuidDataLen = 16;
    constexpr size_t multiRecordHeaderLen = 5;
    /* UUID record data, plus one to skip past the sub-record type byte */
    constexpr size_t uuidRecordData = multiRecordHeaderLen + 1;
    constexpr size_t multiRecordEndOfListMask = 0x80;
    /* The UUID {00112233-4455-6677-8899-AABBCCDDEEFF} would thus be represented
     * as: 0x33 0x22 0x11 0x00 0x55 0x44 0x77 0x66 0x88 0x99 0xAA 0xBB 0xCC 0xDD
     * 0xEE 0xFF
     */
    const std::array<uint8_t, uuidDataLen> uuidCharOrder = {
        3, 2, 1, 0, 5, 4, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15};
    uint32_t areaOffset =
        device.at(getHeaderAreaFieldOffset(fruAreas::fruAreaMultirecord));

    if (areaOffset == 0)
    {
        return;
    }

    areaOffset *= fruBlockSize;
    std::vector<uint8_t>::const_iterator fruBytesIter =
        device.begin() + areaOffset;

    /* Verify area offset */
    if (!verifyOffset(device, fruAreas::fruAreaMultirecord, *fruBytesIter))
    {
        return;
    }
    while (areaOffset + uuidRecordData + uuidDataLen <= device.size())
    {
        if ((areaOffset < device.size()) &&
            (device[areaOffset] ==
             (uint8_t)MultiRecordType::managementAccessRecord))
        {
            if ((areaOffset + multiRecordHeaderLen < device.size()) &&
                (device[areaOffset + multiRecordHeaderLen] ==
                 (uint8_t)SubManagementAccessRecord::systemUniqueID))
            {
                /* Layout of UUID:
                 * source: https://www.ietf.org/rfc/rfc4122.txt
                 *
                 * UUID binary format (16 bytes):
                 * 4B-2B-2B-2B-6B (big endian)
                 *
                 * UUID string is 36 length of characters (36 bytes):
                 * 0        9    14   19   24
                 * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
                 *    be     be   be   be       be
                 * be means it should be converted to big endian.
                 */
                /* Get UUID bytes to UUID string */
                std::stringstream tmp;
                tmp << std::hex << std::setfill('0');
                for (size_t i = 0; i < uuidDataLen; i++)
                {
                    tmp << std::setw(2)
                        << static_cast<uint16_t>(
                               device[areaOffset + uuidRecordData +
                                      uuidCharOrder[i]]);
                }
                std::string uuidStr = tmp.str();
                result["MULTIRECORD_UUID"] =
                    uuidStr.substr(0, 8) + '-' + uuidStr.substr(8, 4) + '-' +
                    uuidStr.substr(12, 4) + '-' + uuidStr.substr(16, 4) + '-' +
                    uuidStr.substr(20, 12);
                break;
            }
        }
        if ((device[areaOffset + 1] & multiRecordEndOfListMask) != 0)
        {
            break;
        }
        areaOffset = areaOffset + device[areaOffset + 2] + multiRecordHeaderLen;
    }
}

resCodes decodeField(
    std::vector<uint8_t>::const_iterator& fruBytesIter,
    std::vector<uint8_t>::const_iterator fruBytesIterEndArea,
    const std::vector<std::string>& fruAreaFieldNames, size_t& fieldIndex,
    DecodeState& state, bool isLangEng, fruAreas area,
    boost::container::flat_map<std::string, std::string>& result)
{
    auto res = decodeFRUData(fruBytesIter, fruBytesIterEndArea, isLangEng);
    state = res.first;
    std::string value = res.second;
    std::string name;
    if (fieldIndex < fruAreaFieldNames.size())
    {
        name = std::string(getFruAreaName(area)) + "_" +
               fruAreaFieldNames.at(fieldIndex);
    }
    else
    {
        name = std::string(getFruAreaName(area)) + "_" + fruCustomFieldName +
               std::to_string(fieldIndex - fruAreaFieldNames.size() + 1);
    }

    if (state == DecodeState::ok)
    {
        // Strip non null characters and trailing spaces from the end
        value.erase(
            std::find_if(value.rbegin(), value.rend(),
                         [](char ch) { return ((ch != 0) && (ch != ' ')); })
                .base(),
            value.end());

        result[name] = std::move(value);
        ++fieldIndex;
    }
    else if (state == DecodeState::err)
    {
        std::cerr << "Error while parsing " << name << "\n";

        // Cancel decoding if failed to parse any of mandatory
        // fields
        if (fieldIndex < fruAreaFieldNames.size())
        {
            std::cerr << "Failed to parse mandatory field \n";
            return resCodes::resErr;
        }
        return resCodes::resWarn;
    }
    else
    {
        if (fieldIndex < fruAreaFieldNames.size())
        {
            std::cerr << "Mandatory fields absent in FRU area "
                      << getFruAreaName(area) << " after " << name << "\n";
            return resCodes::resWarn;
        }
    }
    return resCodes::resOK;
}

resCodes formatIPMIFRU(
    const std::vector<uint8_t>& fruBytes,
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

                unsigned int minutes =
                    *fruBytesIter | *(fruBytesIter + 1) << 8 |
                    *(fruBytesIter + 2) << 16;
                std::tm fruTime = intelEpoch();
                std::time_t timeValue = timegm(&fruTime);
                timeValue += static_cast<long>(minutes) * 60;
                fruTime = *std::gmtime(&timeValue);

                // Tue Nov 20 23:08:00 2018
                std::array<char, 32> timeString = {};
                auto bytes = std::strftime(timeString.data(), timeString.size(),
                                           "%Y%m%dT%H%M%SZ", &fruTime);
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
            ret = decodeField(fruBytesIter, fruBytesIterEndArea,
                              *fruAreaFieldNames, fieldIndex, state, isLangEng,
                              area, result);
            if (ret != resCodes::resOK || ret != resCodes::resWarn)
            {
                return ret;
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

    /* Parsing the Multirecord UUID */
    parseMultirecordUUID(fruBytes, result);

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
unsigned int updateFRUAreaLenAndChecksum(
    std::vector<uint8_t>& fruData, size_t fruAreaStart,
    size_t fruAreaEndOfFieldsOffset, size_t fruAreaEndOffset)
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
        lg2::debug(
            "FRU spec version {VERSION} not supported. Supported version is {SUPPORTED_VERSION}",
            "VERSION", lg2::hex, blockData[0], "SUPPORTED_VERSION", lg2::hex,
            fruVersion);
        return false;
    }

    // verify pad is set to 0
    if (blockData[6] != 0x0)
    {
        lg2::debug("Pad value in header is non zero, value is {VALUE}", "VALUE",
                   lg2::hex, blockData[6]);
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
        lg2::debug(
            "Checksum {CHECKSUM} is invalid. calculated checksum is {CALCULATED_CHECKSUM}",
            "CHECKSUM", lg2::hex, blockData[7], "CALCULATED_CHECKSUM", lg2::hex,
            sum);
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

    lg2::debug("Illegal header {HEADER} base offset {OFFSET}", "HEADER",
               errorHelp, "OFFSET", baseOffset);

    return false;
}

std::pair<std::vector<uint8_t>, bool> readFRUContents(
    FRUReader& reader, const std::string& errorHelp)
{
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData{};
    off_t baseOffset = 0x0;

    if (!findFRUHeader(reader, errorHelp, blockData, baseOffset))
    {
        return {{}, false};
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
            return {{}, true};
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
            return {{}, true};
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
                return {{}, true};
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
            return {{}, true};
        }

        device.insert(device.end(), blockData.begin(),
                      blockData.begin() + requestLength);

        readOffset += requestLength;
        fruLength -= std::min(requestLength, fruLength);
    }

    return {device, true};
}

unsigned int getHeaderAreaFieldOffset(fruAreas area)
{
    return static_cast<unsigned int>(area) + 1;
}

std::vector<uint8_t>& getFRUInfo(const uint16_t& bus, const uint8_t& address)
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

// Iterate FruArea Names and find start and size of the fru area that contains
// the propertyName and the field start location for the property. fruAreaParams
// struct values fruAreaStart, fruAreaSize, fruAreaEnd, fieldLoc values gets
// updated/returned if successful.

bool findFruAreaLocationAndField(std::vector<uint8_t>& fruData,
                                 const std::string& propertyName,
                                 struct FruArea& fruAreaParams)
{
    const std::vector<std::string>* fruAreaFieldNames = nullptr;

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
            std::cerr << "Invalid PropertyName " << propertyName << " \n";
            return false;
    }
    if (fruAreaOffsetFieldValue == 0)
    {
        std::cerr << "FRU Area for " << propertyName << " not present \n";
        return false;
    }

    fruAreaParams.start = fruAreaOffsetFieldValue * fruBlockSize;
    fruAreaParams.size = fruData[fruAreaParams.start + 1] * fruBlockSize;
    fruAreaParams.end = fruAreaParams.start + fruAreaParams.size;
    size_t fruDataIter = fruAreaParams.start + offset;
    size_t skipToFRUUpdateField = 0;
    ssize_t fieldLength = 0;

    bool found = false;
    for (const auto& field : *fruAreaFieldNames)
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
        if (fruDataIter < fruData.size())
        {
            fieldLength = getFieldLength(fruData[fruDataIter]);

            if (fieldLength < 0)
            {
                break;
            }
            fruDataIter += 1 + fieldLength;
        }
    }
    fruAreaParams.updateFieldLoc = fruDataIter;

    return true;
}

// Copy the FRU Area fields and properties into restFRUAreaFieldsData vector.
// Return true for success and false for failure.

bool copyRestFRUArea(std::vector<uint8_t>& fruData,
                     const std::string& propertyName,
                     struct FruArea& fruAreaParams,
                     std::vector<uint8_t>& restFRUAreaFieldsData)
{
    size_t fieldLoc = fruAreaParams.updateFieldLoc;
    size_t start = fruAreaParams.start;
    size_t fruAreaSize = fruAreaParams.size;

    // Push post update fru field bytes to a vector
    ssize_t fieldLength = getFieldLength(fruData[fieldLoc]);
    if (fieldLength < 0)
    {
        std::cerr << "Property " << propertyName << " not present \n";
        return false;
    }

    size_t fruDataIter = 0;
    fruDataIter = fieldLoc;
    fruDataIter += 1 + fieldLength;
    size_t restFRUFieldsLoc = fruDataIter;
    size_t endOfFieldsLoc = 0;

    if (fruDataIter < fruData.size())
    {
        while ((fieldLength = getFieldLength(fruData[fruDataIter])) >= 0)
        {
            if (fruDataIter >= (start + fruAreaSize))
            {
                fruDataIter = start + fruAreaSize;
                break;
            }
            fruDataIter += 1 + fieldLength;
        }
        endOfFieldsLoc = fruDataIter;
    }

    std::copy_n(fruData.begin() + restFRUFieldsLoc,
                endOfFieldsLoc - restFRUFieldsLoc + 1,
                std::back_inserter(restFRUAreaFieldsData));

    fruAreaParams.restFieldsLoc = restFRUFieldsLoc;
    fruAreaParams.restFieldsEnd = endOfFieldsLoc;

    return true;
}

// Get all device dbus path and match path with product name using
// regular expression and find the device index for all devices.

std::optional<int> findIndexForFRU(
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    std::string& productName)
{
    int highest = -1;
    bool found = false;

    for (const auto& busIface : dbusInterfaceMap)
    {
        std::string path = busIface.second->get_object_path();
        if (std::regex_match(path, std::regex(productName + "(_\\d+|)$")))
        {
            // Check if the match named has extra information.
            found = true;
            std::smatch baseMatch;

            bool match = std::regex_match(path, baseMatch,
                                          std::regex(productName + "_(\\d+)$"));
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

    if (!found)
    {
        return std::nullopt;
    }
    return highest;
}

// This function does format fru data as per IPMI format and find the
// productName in the formatted fru data, get that productName and return
// productName if found or return NULL.

std::optional<std::string> getProductName(
    std::vector<uint8_t>& device,
    boost::container::flat_map<std::string, std::string>& formattedFRU,
    uint32_t bus, uint32_t address, size_t& unknownBusObjectCount)
{
    std::string productName;

    resCodes res = formatIPMIFRU(device, formattedFRU);
    if (res == resCodes::resErr)
    {
        std::cerr << "failed to parse FRU for device at bus " << bus
                  << " address " << address << "\n";
        return std::nullopt;
    }
    if (res == resCodes::resWarn)
    {
        std::cerr << "Warnings while parsing FRU for device at bus " << bus
                  << " address " << address << "\n";
    }

    auto productNameFind = formattedFRU.find("BOARD_PRODUCT_NAME");
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
    return productName;
}

bool getFruData(std::vector<uint8_t>& fruData, uint32_t bus, uint32_t address)
{
    try
    {
        fruData = getFRUInfo(static_cast<uint16_t>(bus),
                             static_cast<uint8_t>(address));
    }
    catch (const std::invalid_argument& e)
    {
        std::cerr << "Failure getting FRU Info" << e.what() << "\n";
        return false;
    }

    return !fruData.empty();
}
