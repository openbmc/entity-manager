// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "fru_utils.hpp"

#include "../dbus_regex.hpp"
#include "gzip_utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iomanip>
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
    std::span<const uint8_t>::const_iterator& iter,
    std::span<const uint8_t>::const_iterator& end, bool isLangEng)
{
    std::string value;
    unsigned int i = 0;

    /* we need at least one byte to decode the type/len header */
    if (iter == end)
    {
        lg2::error("Truncated FRU data");
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
        lg2::error("FRU data field extends past end of FRU area data");
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
                lg2::error("Error: Non english string is not supported ");
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
        lg2::error("Warning: languages other than English is not supported");
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
bool verifyOffset(std::span<const uint8_t> fruBytes, fruAreas currentArea,
                  uint8_t len)
{
    unsigned int fruBytesSize = fruBytes.size();

    // check if Fru data has at least 8 byte header
    if (fruBytesSize <= fruBlockSize)
    {
        lg2::error("Error: trying to parse empty FRU");
        return false;
    }

    // Check range of passed currentArea value
    if (currentArea > fruAreas::fruAreaMultirecord)
    {
        lg2::error("Error: Fru area is out of range");
        return false;
    }

    unsigned int currentAreaIndex = getHeaderAreaFieldOffset(currentArea);
    if (currentAreaIndex > fruBytesSize)
    {
        lg2::error("Error: Fru area index is out of range");
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
            lg2::error("Error: Fru area index is out of range");
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
            lg2::error("{AREA1} offset is overlapping with {AREA2} offset",
                       "AREA1", getFruAreaName(currentArea), "AREA2",
                       getFruAreaName(area));
            return false;
        }
    }
    return true;
}

static void parseMultirecordUUID(
    std::span<const uint8_t> device,
    std::flat_map<std::string, std::string, std::less<>>& result)
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
    size_t offset = getHeaderAreaFieldOffset(fruAreas::fruAreaMultirecord);
    if (offset >= device.size())
    {
        throw std::runtime_error("Multirecord UUID offset is out of range");
    }
    uint32_t areaOffset = device[offset];

    if (areaOffset == 0)
    {
        return;
    }

    areaOffset *= fruBlockSize;
    std::span<const uint8_t>::const_iterator fruBytesIter =
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
    std::span<const uint8_t>::const_iterator& fruBytesIter,
    std::span<const uint8_t>::const_iterator& fruBytesIterEndArea,
    const std::vector<std::string>& fruAreaFieldNames, size_t& fieldIndex,
    DecodeState& state, bool isLangEng, const fruAreas& area,
    std::flat_map<std::string, std::string, std::less<>>& result)
{
    auto res = decodeFRUData(fruBytesIter, fruBytesIterEndArea, isLangEng);
    state = res.first;
    std::string value = res.second;
    std::string name;
    bool isCustomField = false;
    if (fieldIndex < fruAreaFieldNames.size())
    {
        name = std::string(getFruAreaName(area)) + "_" +
               fruAreaFieldNames.at(fieldIndex);
    }
    else
    {
        isCustomField = true;
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
        if (isCustomField)
        {
            // Some MAC addresses are stored in a custom field, with
            // "MAC:" prefixed on the value.  If we see that, create a
            // new field with the decoded data
            if (value.starts_with("MAC: "))
            {
                result["MAC_" + name] = value.substr(5);
            }
        }
        result[name] = std::move(value);
        ++fieldIndex;
    }
    else if (state == DecodeState::err)
    {
        lg2::error("Error while parsing {NAME}", "NAME", name);

        // Cancel decoding if failed to parse any of mandatory
        // fields
        if (fieldIndex < fruAreaFieldNames.size())
        {
            lg2::error("Failed to parse mandatory field ");
            return resCodes::resErr;
        }
        return resCodes::resWarn;
    }
    else
    {
        if (fieldIndex < fruAreaFieldNames.size())
        {
            lg2::error(
                "Mandatory fields absent in FRU area {AREA} after {NAME}",
                "AREA", getFruAreaName(area), "NAME", name);
            return resCodes::resWarn;
        }
    }
    return resCodes::resOK;
}

resCodes formatIPMIFRU(
    std::span<const uint8_t> fruBytes,
    std::flat_map<std::string, std::string, std::less<>>& result)
{
    resCodes ret = resCodes::resOK;
    if (fruBytes.size() <= fruBlockSize)
    {
        lg2::error("Error: trying to parse empty FRU ");
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
        std::span<const uint8_t>::const_iterator fruBytesIter =
            fruBytes.begin() + offset;
        if (fruBytesIter + fruBlockSize >= fruBytes.end())
        {
            lg2::error("Not enough data to parse ");
            return resCodes::resErr;
        }
        // check for format version 1
        if (*fruBytesIter != 0x01)
        {
            lg2::error("Unexpected version {VERSION}", "VERSION",
                       *fruBytesIter);
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
        std::span<const uint8_t>::const_iterator fruBytesIterEndArea =
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
            lg2::error("{ERR}", "ERR", ss.str());
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
                    lg2::error("invalid time string encountered");
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
                lg2::error(
                    "Internal error: unexpected FRU area index: {INDEX} ",
                    "INDEX", static_cast<int>(area));
                return resCodes::resErr;
            }
        }
        size_t fieldIndex = 0;
        DecodeState state = DecodeState::ok;
        do
        {
            resCodes decodeRet = decodeField(fruBytesIter, fruBytesIterEndArea,
                                             *fruAreaFieldNames, fieldIndex,
                                             state, isLangEng, area, result);
            if (decodeRet == resCodes::resErr)
            {
                return resCodes::resErr;
            }
            if (decodeRet == resCodes::resWarn)
            {
                ret = decodeRet;
            }
        } while (state == DecodeState::ok);
        for (; fruBytesIter < fruBytesIterEndArea; fruBytesIter++)
        {
            uint8_t c = *fruBytesIter;
            if (c != 0U)
            {
                lg2::error("Non-zero byte after EndOfFields in FRU area {AREA}",
                           "AREA", getFruAreaName(area));
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
uint8_t calculateChecksum(std::span<const uint8_t>::const_iterator iter,
                          std::span<const uint8_t>::const_iterator end)
{
    constexpr int checksumMod = 256;
    uint8_t sum = std::accumulate(iter, end, static_cast<uint8_t>(0));
    return (checksumMod - sum) % checksumMod;
}

uint8_t calculateChecksum(std::span<const uint8_t> fruAreaData)
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

std::string parseMacFromGzipXmlHeader(FRUReader& reader, off_t offset)
{
    // gzip starts at offset 512. Read that from the FRU
    // in this case, 32k bytes is enough to hold the whole manifest
    constexpr size_t totalReadSize = 32UL * 1024UL;

    std::vector<uint8_t> headerData(totalReadSize, 0U);

    int rc = reader.read(offset, totalReadSize, headerData.data());
    if (rc <= 0)
    {
        return {};
    }

    std::optional<std::string> xml = gzipInflate(headerData);
    if (!xml)
    {
        return {};
    }
    std::vector<std::string> node = getNodeFromXml(
        *xml, "/GSSKU/BoardInfo/Main/NIC/*[Mode = 'Dedicated']/MacAddr0");
    if (node.empty())
    {
        lg2::debug("No mac address found in gzip xml header");
        return {};
    }
    if (node.size() > 1)
    {
        lg2::warning("Multiple mac addresses found in gzip xml header");
    }
    return node[0];
}

std::optional<FruSections> findFRUHeader(
    FRUReader& reader, const std::string& errorHelp, off_t startingOffset)
{
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData = {};
    if (reader.read(startingOffset, 0x8, blockData.data()) < 0)
    {
        lg2::error("failed to read {ERR} base offset {OFFSET}", "ERR",
                   errorHelp, "OFFSET", startingOffset);
        return std::nullopt;
    }

    // check the header checksum
    if (validateHeader(blockData))
    {
        FruSections fru = {};
        static_assert(fru.ipmiFruBlock.size() == blockData.size(),
                      "size mismatch in block data");
        std::memcpy(fru.ipmiFruBlock.data(), blockData.data(),
                    I2C_SMBUS_BLOCK_MAX);
        fru.IpmiFruOffset = startingOffset;
        return fru;
    }

    // only continue the search if we just looked at 0x0.
    if (startingOffset != 0)
    {
        return std::nullopt;
    }

    // now check for special cases where the IPMI data is at an offset

    // check if blockData starts with tyanHeader
    const std::vector<uint8_t> tyanHeader = {'$', 'T', 'Y', 'A', 'N', '$'};
    if (blockData.size() >= tyanHeader.size() &&
        std::equal(tyanHeader.begin(), tyanHeader.end(), blockData.begin()))
    {
        // look for the FRU header at offset 0x6000
        off_t tyanOffset = 0x6000;
        return findFRUHeader(reader, errorHelp, tyanOffset);
    }

    // check if blockData starts with gigabyteHeader
    const std::vector<uint8_t> gigabyteHeader = {'G', 'I', 'G', 'A',
                                                 'B', 'Y', 'T', 'E'};
    if (blockData.size() >= gigabyteHeader.size() &&
        std::equal(gigabyteHeader.begin(), gigabyteHeader.end(),
                   blockData.begin()))
    {
        // look for the FRU header at offset 0x4000
        off_t gbOffset = 0x4000;
        auto sections = findFRUHeader(reader, errorHelp, gbOffset);
        if (sections)
        {
            lg2::debug("succeeded on GB parse");
            // GB xml header is at 512 bytes
            sections->GigabyteXmlOffset = 512;
        }
        else
        {
            lg2::error("Failed on GB parse");
        }
        return sections;
    }

    lg2::debug("Illegal header {HEADER} base offset {OFFSET}", "HEADER",
               errorHelp, "OFFSET", startingOffset);

    return std::nullopt;
}

std::pair<std::vector<uint8_t>, bool> readFRUContents(
    FRUReader& reader, const std::string& errorHelp)
{
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData{};
    std::optional<FruSections> sections = findFRUHeader(reader, errorHelp, 0);
    if (!sections)
    {
        return {{}, false};
    }
    const off_t baseOffset = sections->IpmiFruOffset;
    std::memcpy(blockData.data(), sections->ipmiFruBlock.data(),
                blockData.size());
    std::vector<uint8_t> device;
    device.insert(device.end(), blockData.begin(),
                  std::next(blockData.begin(), 8));

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
            lg2::error(
                "Fru area offsets are not in required order as per Section 17 of Fru specification");
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
            lg2::error("failed to read {ERR} base offset {OFFSET}", "ERR",
                       errorHelp, "OFFSET", baseOffset);
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
                lg2::error("failed to read {STR} base offset {OFFSET}", "STR",
                           errorHelp, "OFFSET", baseOffset);
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
            lg2::error("failed to read {ERR} base offset {OFFSET}", "ERR",
                       errorHelp, "OFFSET", baseOffset);
            return {{}, true};
        }

        device.insert(device.end(), blockData.begin(),
                      std::next(blockData.begin(), requestLength));

        readOffset += requestLength;
        fruLength -= std::min(requestLength, fruLength);
    }

    if (sections->GigabyteXmlOffset != 0)
    {
        std::string macAddress =
            parseMacFromGzipXmlHeader(reader, sections->GigabyteXmlOffset);
        if (!macAddress.empty())
        {
            // launder the mac address as we expect into
            // BOARD_INFO_AM2 to allow the rest of the
            // system to use it
            std::string mac = std::format("MAC: {}", macAddress);
            updateAddProperty(mac, "BOARD_INFO_AM2", device);
        }
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

static bool updateHeaderChecksum(std::vector<uint8_t>& fruData)
{
    if (fruData.size() < fruBlockSize)
    {
        lg2::debug("FRU data is too small to contain a valid header.");
        return false;
    }

    uint8_t& checksumInBytes = fruData[7];
    uint8_t checksum =
        calculateChecksum({fruData.begin(), fruData.begin() + 7});
    std::swap(checksumInBytes, checksum);

    if (checksumInBytes != checksum)
    {
        lg2::debug(
            "FRU header checksum updated from {OLD_CHECKSUM} to {NEW_CHECKSUM}",
            "OLD_CHECKSUM", static_cast<int>(checksum), "NEW_CHECKSUM",
            static_cast<int>(checksumInBytes));
    }
    return true;
}

bool updateAreaChecksum(std::vector<uint8_t>& fruArea)
{
    if (fruArea.size() < fruBlockSize)
    {
        lg2::debug("FRU area is too small to contain a valid header.");
        return false;
    }
    if (fruArea.size() % fruBlockSize != 0)
    {
        lg2::debug("FRU area size is not a multiple of {SIZE} bytes.", "SIZE",
                   fruBlockSize);
        return false;
    }

    uint8_t oldcksum = fruArea[fruArea.size() - 1];

    fruArea[fruArea.size() - 1] =
        0; // Reset checksum byte to 0 before recalculating
    fruArea[fruArea.size() - 1] = calculateChecksum(fruArea);

    if (oldcksum != fruArea[fruArea.size() - 1])
    {
        lg2::debug(
            "FRU area checksum updated from {OLD_CHECKSUM} to {NEW_CHECKSUM}",
            "OLD_CHECKSUM", static_cast<int>(oldcksum), "NEW_CHECKSUM",
            static_cast<int>(fruArea[fruArea.size() - 1]));
    }
    return true;
}

static std::optional<size_t> calculateAreaSize(
    fruAreas area, std::span<const uint8_t> fruData, size_t areaOffset)
{
    switch (area)
    {
        case fruAreas::fruAreaChassis:
        case fruAreas::fruAreaBoard:
        case fruAreas::fruAreaProduct:
            if (areaOffset + 1 >= fruData.size())
            {
                return std::nullopt;
            }
            return fruData[areaOffset + 1] * fruBlockSize; // Area size in bytes
        case fruAreas::fruAreaInternal:
        {
            // Internal area size: It is difference between the next area
            // offset and current area offset
            for (fruAreas areaIt = fruAreas::fruAreaChassis;
                 areaIt <= fruAreas::fruAreaMultirecord; ++areaIt)
            {
                size_t headerOffset = getHeaderAreaFieldOffset(areaIt);
                if (headerOffset >= fruData.size())
                {
                    return std::nullopt;
                }
                size_t nextAreaOffset = fruData[headerOffset];
                if (nextAreaOffset != 0)
                {
                    return nextAreaOffset * fruBlockSize - areaOffset;
                }
            }
            return std::nullopt;
        }
        break;
        case fruAreas::fruAreaMultirecord:
            // Multirecord area size.
            return fruData.size() - areaOffset; // Area size in bytes
        default:
            lg2::error("Invalid FRU area: {AREA}", "AREA",
                       static_cast<int>(area));
    }
    return std::nullopt;
}

static size_t getBlockCount(size_t byteCount)
{
    size_t blocks = (byteCount + fruBlockSize - 1) / fruBlockSize;
    // if we're perfectly aligned, we need another block for the checksum
    if ((byteCount % fruBlockSize) == 0)
    {
        blocks++;
    }
    return blocks;
}

bool disassembleFruData(std::vector<uint8_t>& fruData,
                        std::vector<std::vector<uint8_t>>& areasData)
{
    if (fruData.size() < 8)
    {
        lg2::debug("FRU data is too small to contain a valid header.");
        return false;
    }

    // Clear areasData before disassembling
    areasData.clear();

    // Iterate through all areas & store each area data in a vector.
    for (fruAreas area = fruAreas::fruAreaInternal;
         area <= fruAreas::fruAreaMultirecord; ++area)
    {
        size_t areaOffset = fruData[getHeaderAreaFieldOffset(area)];

        if (areaOffset == 0)
        {
            // Store empty area data for areas that are not present
            areasData.emplace_back();
            continue;               // Skip areas that are not present
        }
        areaOffset *= fruBlockSize; // Convert to byte offset

        std::optional<size_t> areaSize =
            calculateAreaSize(area, fruData, areaOffset);
        if (!areaSize)
        {
            return false;
        }

        if ((areaOffset + *areaSize) > fruData.size())
        {
            lg2::error("Area offset + size exceeds FRU data size.");
            return false;
        }

        areasData.emplace_back(fruData.begin() + areaOffset,
                               fruData.begin() + areaOffset + *areaSize);
    }

    return true;
}

struct FieldInfo
{
    size_t length;
    size_t index;
};

static std::optional<FieldInfo> findOrCreateField(
    std::vector<uint8_t>& areaData, const std::string& propertyName,
    const fruAreas& fruAreaToUpdate)
{
    int fieldIndex = 0;
    int fieldLength = 0;
    std::string areaName = propertyName.substr(0, propertyName.find('_'));
    std::string propertyNamePrefix = areaName + "_";
    const std::vector<std::string>* fruAreaFieldNames = nullptr;

    switch (fruAreaToUpdate)
    {
        case fruAreas::fruAreaChassis:
            fruAreaFieldNames = &chassisFruAreas;
            fieldIndex = 3;
            break;
        case fruAreas::fruAreaBoard:
            fruAreaFieldNames = &boardFruAreas;
            fieldIndex = 6;
            break;
        case fruAreas::fruAreaProduct:
            fruAreaFieldNames = &productFruAreas;
            fieldIndex = 3;
            break;
        default:
            lg2::info("Invalid FRU area: {AREA}", "AREA",
                      static_cast<int>(fruAreaToUpdate));
            return std::nullopt;
    }

    for (const auto& field : *fruAreaFieldNames)
    {
        fieldLength = getFieldLength(areaData[fieldIndex]);
        if (fieldLength < 0)
        {
            areaData.insert(areaData.begin() + fieldIndex, 0xc0);
            fieldLength = 0;
        }

        if (propertyNamePrefix + field == propertyName)
        {
            return FieldInfo{static_cast<size_t>(fieldLength),
                             static_cast<size_t>(fieldIndex)};
        }
        fieldIndex += 1 + fieldLength;
    }

    size_t pos = propertyName.find(fruCustomFieldName);
    if (pos == std::string::npos)
    {
        return std::nullopt;
    }

    // Get field after pos
    std::string customFieldIdx =
        propertyName.substr(pos + fruCustomFieldName.size());

    // Check if customFieldIdx is a number
    if (!std::all_of(customFieldIdx.begin(), customFieldIdx.end(), ::isdigit))
    {
        return std::nullopt;
    }

    size_t customFieldIndex = std::stoi(customFieldIdx);

    // insert custom fields up to the index we want
    for (size_t i = 0; i < customFieldIndex; i++)
    {
        fieldLength = getFieldLength(areaData[fieldIndex]);
        if (fieldLength < 0)
        {
            areaData.insert(areaData.begin() + fieldIndex, 0xc0);
            fieldLength = 0;
        }
        fieldIndex += 1 + fieldLength;
    }

    fieldIndex -= (fieldLength + 1);
    fieldLength = getFieldLength(areaData[fieldIndex]);
    return FieldInfo{static_cast<size_t>(fieldLength),
                     static_cast<size_t>(fieldIndex)};
}

static std::optional<size_t> findEndOfFieldMarker(std::span<uint8_t> bytes)
{
    // we're skipping the checksum
    // this function assumes a properly sized and formatted area
    static uint8_t constexpr endOfFieldsByte = 0xc1;
    for (int index = bytes.size() - 2; index >= 0; --index)
    {
        if (bytes[index] == endOfFieldsByte)
        {
            return index;
        }
    }
    return std::nullopt;
}

static std::optional<size_t> getNonPaddedSizeOfArea(std::span<uint8_t> bytes)
{
    if (auto endOfFields = findEndOfFieldMarker(bytes))
    {
        return *endOfFields + 1;
    }
    return std::nullopt;
}

bool setField(const fruAreas& fruAreaToUpdate, std::vector<uint8_t>& areaData,
              const std::string& propertyName, const std::string& value)
{
    if (value.size() == 1 || value.size() > 63)
    {
        lg2::error("Invalid value {VALUE} for field {PROP}", "VALUE", value,
                   "PROP", propertyName);
        return false;
    }

    // This is inneficient, but the alternative requires
    // a bunch of complicated indexing and search to
    // figure out if we cross a block boundary
    // if we feel that this is too inneficient in the future,
    // we can implement that.
    std::vector<uint8_t> tmpBuffer = areaData;

    auto fieldInfo =
        findOrCreateField(tmpBuffer, propertyName, fruAreaToUpdate);

    if (!fieldInfo)
    {
        lg2::error("Field {FIELD} not found in area {AREA}", "FIELD",
                   propertyName, "AREA", getFruAreaName(fruAreaToUpdate));
        return false;
    }

    auto fieldIt = tmpBuffer.begin() + fieldInfo->index;
    // Erase the existing field content.
    tmpBuffer.erase(fieldIt, fieldIt + fieldInfo->length + 1);
    // Insert the new field value
    tmpBuffer.insert(fieldIt, 0xc0 | value.size());
    tmpBuffer.insert_range(fieldIt + 1, value);

    auto newSize = getNonPaddedSizeOfArea(tmpBuffer);
    auto oldSize = getNonPaddedSizeOfArea(areaData);

    if (!oldSize || !newSize)
    {
        lg2::error("Failed to find the size of the area");
        return false;
    }

    size_t newSizePadded = getBlockCount(*newSize);
#ifndef ENABLE_FRU_AREA_RESIZE

    size_t oldSizePadded = getBlockCount(*oldSize);

    if (newSizePadded != oldSizePadded)
    {
        lg2::error(
            "FRU area {AREA} resize is disabled, cannot increase size from {OLD_SIZE} to {NEW_SIZE}",
            "AREA", getFruAreaName(fruAreaToUpdate), "OLD_SIZE",
            static_cast<int>(oldSizePadded), "NEW_SIZE",
            static_cast<int>(newSizePadded));
        return false;
    }
#endif
    // Resize the buffer as per numOfBlocks & pad with zeros
    tmpBuffer.resize(newSizePadded * fruBlockSize, 0);

    // Update the length field
    tmpBuffer[1] = newSizePadded;
    updateAreaChecksum(tmpBuffer);

    areaData = std::move(tmpBuffer);

    return true;
}

bool assembleFruData(std::vector<uint8_t>& fruData,
                     const std::vector<std::vector<uint8_t>>& areasData)
{
    for (const auto& area : areasData)
    {
        if ((area.size() % fruBlockSize) != 0U)
        {
            lg2::error("unaligned area sent to assembleFruData");
            return false;
        }
    }

    // Clear the existing FRU data
    fruData.clear();
    fruData.resize(8); // Start with the header size

    // Write the header
    fruData[0] = fruVersion; // Version
    fruData[1] = 0;          // Internal area offset
    fruData[2] = 0;          // Chassis area offset
    fruData[3] = 0;          // Board area offset
    fruData[4] = 0;          // Product area offset
    fruData[5] = 0;          // Multirecord area offset
    fruData[6] = 0;          // Pad
    fruData[7] = 0;          // Checksum (to be updated later)

    size_t writeOffset = 8;  // Start writing after the header

    for (fruAreas area = fruAreas::fruAreaInternal;
         area <= fruAreas::fruAreaMultirecord; ++area)
    {
        const auto& areaBytes = areasData[static_cast<size_t>(area)];

        if (areaBytes.empty())
        {
            lg2::debug("Skipping empty area: {AREA}", "AREA",
                       getFruAreaName(area));
            continue; // Skip areas that are not present
        }

        // Set the area offset in the header
        fruData[getHeaderAreaFieldOffset(area)] = writeOffset / fruBlockSize;
        fruData.append_range(areaBytes);
        writeOffset += areaBytes.size();
    }

    // Update the header checksum
    if (!updateHeaderChecksum(fruData))
    {
        lg2::error("failed to update header checksum");
        return false;
    }

    return true;
}

// Create a dummy area in areData variable based on specified fruArea
bool createDummyArea(fruAreas fruArea, std::vector<uint8_t>& areaData)
{
    uint8_t numOfFields = 0;
    uint8_t numOfBlocks = 0;
    // Clear the areaData vector
    areaData.clear();

    // Set the version, length, and other fields
    areaData.push_back(fruVersion); // Version 1
    areaData.push_back(0);          // Length (to be updated later)

    switch (fruArea)
    {
        case fruAreas::fruAreaChassis:
            areaData.push_back(0x00); // Chassis type
            numOfFields = chassisFruAreas.size();
            break;
        case fruAreas::fruAreaBoard:
            areaData.push_back(0x00); // Board language code (default)
            areaData.insert(areaData.end(),
                            {0x00, 0x00,
                             0x00}); // Board manufacturer date (default)
            numOfFields = boardFruAreas.size();
            break;
        case fruAreas::fruAreaProduct:
            areaData.push_back(0x00); // Product language code (default)
            numOfFields = productFruAreas.size();
            break;
        default:
            lg2::debug("Invalid FRU area to create: {AREA}", "AREA",
                       static_cast<int>(fruArea));
            return false;
    }

    for (size_t i = 0; i < numOfFields; ++i)
    {
        areaData.push_back(0xc0); // Empty field type
    }

    // Add EndOfFields marker
    areaData.push_back(0xC1);
    numOfBlocks = (areaData.size() + fruBlockSize - 1) /
                  fruBlockSize; // Calculate number of blocks needed
    areaData.resize(numOfBlocks * fruBlockSize, 0); // Fill with zeros
    areaData[1] = numOfBlocks;                      // Update length field
    updateAreaChecksum(areaData);

    return true;
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
        lg2::error("Can't parse area name for property {PROP} ", "PROP",
                   propertyName);
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
            lg2::error("Invalid PropertyName {PROP}", "PROP", propertyName);
            return false;
    }
    if (fruAreaOffsetFieldValue == 0)
    {
        lg2::error("FRU Area for {PROP} not present ", "PROP", propertyName);
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
            lg2::error("PropertyName doesn't exist in FRU Area Vectors: {PROP}",
                       "PROP", propertyName);
            return false;
        }
        std::string fieldNumStr =
            propertyName.substr(pos + fruCustomFieldName.length());
        size_t fieldNum = std::stoi(fieldNumStr);
        if (fieldNum == 0)
        {
            lg2::error("PropertyName not recognized: {PROP}", "PROP",
                       propertyName);
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
        lg2::error("Property {PROP} not present ", "PROP", propertyName);
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
    std::flat_map<std::pair<size_t, size_t>,
                  std::shared_ptr<sdbusplus::asio::dbus_interface>>&
        dbusInterfaceMap,
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
    std::flat_map<std::string, std::string, std::less<>>& formattedFRU,
    uint32_t bus, uint32_t address, size_t& unknownBusObjectCount)
{
    std::string productName;

    resCodes res = formatIPMIFRU(device, formattedFRU);
    if (res == resCodes::resErr)
    {
        lg2::error("failed to parse FRU for device at bus {BUS} address {ADDR}",
                   "BUS", bus, "ADDR", address);
        return std::nullopt;
    }
    if (res == resCodes::resWarn)
    {
        lg2::error(
            "Warnings while parsing FRU for device at bus {BUS} address {ADDR}",
            "BUS", bus, "ADDR", address);
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
        productName =
            dbus_regex::sanitizeForDBusMember(productNameFind->second);
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
        lg2::error("Failure getting FRU Info: {ERR}", "ERR", e);
        return false;
    }

    return !fruData.empty();
}

bool isFieldEditable(std::string_view fieldName)
{
    if (fieldName == "PRODUCT_ASSET_TAG")
    {
        return true; // PRODUCT_ASSET_TAG is always editable.
    }

    if (!ENABLE_FRU_UPDATE_PROPERTY)
    {
        return false; // If FRU update is disabled, no fields are editable.
    }

    // Editable fields
    constexpr std::array<std::string_view, 8> editableFields = {
        "MANUFACTURER",  "PRODUCT_NAME", "PART_NUMBER",    "VERSION",
        "SERIAL_NUMBER", "ASSET_TAG",    "FRU_VERSION_ID", "INFO_AM"};

    // Find position of first underscore
    std::size_t pos = fieldName.find('_');
    if (pos == std::string_view::npos || pos + 1 >= fieldName.size())
    {
        return false;
    }

    // Extract substring after the underscore
    std::string_view subField = fieldName.substr(pos + 1);

    // Trim trailing digits
    while (!subField.empty() && (std::isdigit(subField.back()) != 0))
    {
        subField.remove_suffix(1);
    }

    // Match against editable fields
    return std::ranges::contains(editableFields, subField);
}

bool updateAddProperty(const std::string& propertyValue,
                       const std::string& propertyName,
                       std::vector<uint8_t>& fruData)
{
    // Validate field length: must be 2–63 characters
    const size_t len = propertyValue.length();
    if (len == 1 || len > 63)
    {
        lg2::error(
            "FRU field data must be 0 or between 2 and 63 characters. Invalid Length: {LEN}",
            "LEN", len);
        return false;
    }

    if (fruData.empty())
    {
        lg2::error("Empty FRU data\n");
        return false;
    }

    // Extract area name (prefix before underscore)
    std::string areaName = propertyName.substr(0, propertyName.find('_'));
    auto areaIterator =
        std::find(fruAreaNames.begin(), fruAreaNames.end(), areaName);
    if (areaIterator == fruAreaNames.end())
    {
        lg2::error("Failed to get FRU area for property: {AREA}", "AREA",
                   areaName);
        return false;
    }

    fruAreas fruAreaToUpdate = static_cast<fruAreas>(
        std::distance(fruAreaNames.begin(), areaIterator));

    std::vector<std::vector<uint8_t>> areasData;
    if (!disassembleFruData(fruData, areasData))
    {
        lg2::error("Failed to disassemble Fru Data");
        return false;
    }

    std::vector<uint8_t>& areaData =
        areasData[static_cast<size_t>(fruAreaToUpdate)];
    if (areaData.empty())
    {
        // If ENABLE_FRU_AREA_RESIZE is not defined then return with failure
#ifndef ENABLE_FRU_AREA_RESIZE
        lg2::error(
            "FRU area {AREA} not present and ENABLE_FRU_AREA_RESIZE is not set. "
            "Returning failure.",
            "AREA", areaName);
        return false;
#endif
        if (!createDummyArea(fruAreaToUpdate, areaData))
        {
            lg2::error("Failed to create dummy area for {AREA}", "AREA",
                       areaName);
            return false;
        }
    }

    if (!setField(fruAreaToUpdate, areaData, propertyName, propertyValue))
    {
        lg2::error("Failed to set field value for property: {PROPERTY}",
                   "PROPERTY", propertyName);
        return false;
    }

    if (!assembleFruData(fruData, areasData))
    {
        lg2::error("Failed to reassemble FRU data");
        return false;
    }

    if (fruData.empty())
    {
        lg2::error("FRU data is empty after assembly");
        return false;
    }

    return true;
}
