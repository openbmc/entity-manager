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

#include "Common.hpp"

#include "Utils.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <array>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

bool isMuxBus(size_t bus)
{
    return is_symlink(std::filesystem::path(
        "/sys/bus/i2c/devices/i2c-" + std::to_string(bus) + "/mux_device"));
}

const std::tm intelEpoch(void)
{
    std::tm val = {};
    val.tm_year = 1996 - 1900;
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
                  const std::vector<uint8_t>::const_iterator& end)
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

void checkLang(uint8_t lang)
{
    // If Lang is not English then the encoding is defined as 2-byte UNICODE,
    // but we don't support that.
    if (lang && lang != 25)
    {
        std::cerr << "Warning: language other then English is not "
                     "supported \n";
    }
}

bool formatFRU(const std::vector<uint8_t>& fruBytes,
               boost::container::flat_map<std::string, std::string>& result)
{
    if (fruBytes.size() <= fruBlockSize)
    {
        std::cerr << "Error: trying to parse empty FRU \n";
        return false;
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
            return false;
        }
        // check for format version 1
        if (*fruBytesIter != 0x01)
        {
            std::cerr << "Unexpected version " << *fruBytesIter << "\n";
            return false;
        }
        ++fruBytesIter;
        uint8_t fruAreaSize = *fruBytesIter * fruBlockSize;
        std::vector<uint8_t>::const_iterator fruBytesIterEndArea =
            fruBytes.begin() + offset + fruAreaSize - 1;
        ++fruBytesIter;

        if (calculateChecksum(fruBytes.begin() + offset, fruBytesIterEndArea) !=
            *fruBytesIterEndArea)
        {
            std::cerr << "Checksum error in FRU area " << getFruAreaName(area)
                      << "\n";
            return false;
        }

        switch (area)
        {
            case fruAreas::fruAreaChassis:
            {
                result["CHASSIS_TYPE"] =
                    std::to_string(static_cast<int>(*fruBytesIter));
                fruBytesIter += 1;
                fruAreaFieldNames = &CHASSIS_FRU_AREAS;
                break;
            }
            case fruAreas::fruAreaBoard:
            {
                uint8_t lang = *fruBytesIter;
                result["BOARD_LANGUAGE_CODE"] =
                    std::to_string(static_cast<int>(lang));
                checkLang(lang);
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
                    return false;
                }

                result["BOARD_MANUFACTURE_DATE"] = std::string(timeString);
                fruBytesIter += 3;
                fruAreaFieldNames = &BOARD_FRU_AREAS;
                break;
            }
            case fruAreas::fruAreaProduct:
            {
                uint8_t lang = *fruBytesIter;
                result["PRODUCT_LANGUAGE_CODE"] =
                    std::to_string(static_cast<int>(lang));
                checkLang(lang);
                fruBytesIter += 1;
                fruAreaFieldNames = &PRODUCT_FRU_AREAS;
                break;
            }
            default:
            {
                std::cerr << "Internal error: unexpected FRU area index: "
                          << static_cast<int>(area) << " \n";
                return false;
            }
        }
        size_t fieldIndex = 0;
        DecodeState state;
        do
        {
            auto res = decodeFRUData(fruBytesIter, fruBytesIterEndArea);
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
                    FRU_CUSTOM_FIELD_NAME +
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
                // Cancel decoding if failed to parse any of mandatory
                // fields
                if (fieldIndex < fruAreaFieldNames->size())
                {
                    std::cerr << "Failed to parse mandatory field \n";
                    return false;
                }
            }
            else
            {
                if (fieldIndex < fruAreaFieldNames->size())
                {
                    std::cerr << "Mandatory fields absent in FRU area "
                              << getFruAreaName(area) << " after " << name
                              << "\n";
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
                break;
            }
        }
    }

    return true;
}

// Calculate new checksum for fru info area
uint8_t calculateChecksum(std::vector<uint8_t>::const_iterator iter,
                          std::vector<uint8_t>::const_iterator end)
{
    constexpr int checksumMod = 256;
    constexpr uint8_t modVal = 0xFF;
    int sum = std::accumulate(iter, end, 0);
    int checksum = (checksumMod - sum) & modVal;
    return static_cast<uint8_t>(checksum);
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
    else
    {
        return fruFieldTypeLenValue & typeLenMask;
    }
}
