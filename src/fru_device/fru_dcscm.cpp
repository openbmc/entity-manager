/*
// Copyright (c) 2025 Intel Corporation
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
/// \file fru_dcscm.cpp

#include "fru_dcscm.hpp"

#include <phosphor-logging/lg2.hpp>

#include <iostream>
#include <sstream>

namespace dcscm_fru
{
enum MultiRecordType : uint8_t
{
    hpmFruMultiRecordArea = 0xC1
};

/* Change the FRU data into a std::string, given an input iterator and
 * mulirecord pair. If the state returned is fruDataOk, then the resulting
 * string is updated to property value.
 */
std::pair<DecodeState, std::string> formatString(
    std::span<const uint8_t>::const_iterator& iter,
    const std::pair<std::string, uint8_t>& mRecordPair)
{
    std::string value;
    int i = 0;
    std::stringstream ss;
    uint8_t val = 0;
    std::string multiRecordStr = mRecordPair.first;
    uint8_t len = mRecordPair.second;

    ss << std::hex << std::setfill('0');
    if (len == 0)
    {
        lg2::error("Truncated FRU data");
        return make_pair(DecodeState::err, value);
    }

    if (len == 1)
    {
        /* DC-SCI and LTPI major & minor version are stored in each nibble:
        7:4 - Major number, 3:0 - Minor number*/
        if ((multiRecordStr == "DC_SCI_VERSION") ||
            (multiRecordStr == "LTPI_VERSION"))
        {
            val = static_cast<uint8_t>(*iter);
            ss << std::setw(1) << static_cast<int>(val >> 4) << ".";
            ss << std::setw(1) << static_cast<int>(val & 0xf);
            value = ss.str();
        }
        else
        {
            val = static_cast<uint8_t>(*iter);
            ss << std::setw(1) << static_cast<int>(val);
            value = ss.str();
        }
    }
    else
    {
        /* DC-SCI and LTPI major & minor Revisions are stored in 2 bytes:
        15:8 - Major number, 7:0 - Minor number*/
        if ((multiRecordStr == "DC_SCI_REVISION") ||
            (multiRecordStr == "LTPI_REVISION"))
        {
            val = static_cast<uint8_t>(*iter);
            ss << std::setw(1) << static_cast<int>(val) << ".";
            val = static_cast<uint8_t>(*(iter + 1));
            ss << std::setw(1) << static_cast<int>(val);
            value = ss.str();
        }
        else
        {
            /*MSB stored at higher address and LSB at lower address*/
            ss << "0x";
            for (i = len - 1; i >= 0; i--)
            {
                val = static_cast<uint8_t>(*(iter + i));
                ss << std::setw(2) << static_cast<int>(val);
            }
            value = ss.str();
        }
    }
    return make_pair(DecodeState::ok, value);
}

void printChecksumError(uint8_t computedChecksum, uint8_t actualChecksum)
{
    std::stringstream ss;
    std::string errorString;
    ss << std::hex << std::setfill('0');
    ss << "Checksum error in Multi Record FRU area "
       << "\n";
    ss << "\tComputed checksum: 0x" << std::setw(2)
       << static_cast<int>(computedChecksum) << "\n";
    ss << "\tThe read checksum: 0x" << std::setw(2)
       << static_cast<int>(actualChecksum) << "\n";
    errorString = ss.str();
    lg2::error("FRU Checksum Error!! '{errorString}'", "ERROR_STRING",
               errorString);
    return;
}

/* Formats the Multirecord fru data as per OCP DC-SCM Rev 2.1 Ver1.1,
 * given an input fru data device. Expose the formatted fru data on
 * D-bus via fru-device service through result container.
 */
void populateMultirecordOCPDCSCM(
    std::span<const uint8_t> device,
    boost::container::flat_map<std::string, std::string>& result)
{
    constexpr size_t multiRecordHeaderLen = 5;
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
    /* Processing for HPM FRU Multi-record area */
    if ((areaOffset < device.size()) &&
        (device[areaOffset] == (uint8_t)MultiRecordType::hpmFruMultiRecordArea))
    {
        /*Verify Header Checksum*/
        std::span<const uint8_t>::const_iterator fruBytesIterEndArea =
            device.begin() + areaOffset + multiRecordHeaderLen - 1;
        uint8_t fruComputedChecksum =
            calculateChecksum(device.begin() + areaOffset, fruBytesIterEndArea);
        if (fruComputedChecksum != *fruBytesIterEndArea)
        {
            printChecksumError(fruComputedChecksum, *fruBytesIterEndArea);
            return;
        }

        /*Verify Record Checksum*/
        uint32_t areaLength = device[areaOffset + 2];
        if ((areaLength == 0) ||
            (areaOffset + multiRecordHeaderLen + areaLength - 1 >
             device.size()))
        {
            lg2::error("Record Length mismatch Error");
            return;
        }
        uint32_t frureadChecksum = device[areaOffset + 3];
        fruBytesIterEndArea =
            device.begin() + areaOffset + multiRecordHeaderLen + areaLength;
        fruComputedChecksum = calculateChecksum(
            device.begin() + areaOffset + multiRecordHeaderLen,
            fruBytesIterEndArea);
        if (fruComputedChecksum != frureadChecksum)
        {
            printChecksumError(fruComputedChecksum, frureadChecksum);
            return;
        }

        /*Format the fru record hex values to understandable strings*/
        fruBytesIter += multiRecordHeaderLen;
        for (const auto& multiRecordPair : hpmFruMuitiRecordTable)
        {
            auto res = formatString(fruBytesIter, multiRecordPair);
            fruBytesIter += multiRecordPair.second;
            std::string name;
            name = std::string(getFruAreaName(fruAreas::fruAreaMultirecord)) +
                   "_" + multiRecordPair.first;
            if (res.first == DecodeState::ok)
            {
                result[name] = std::move(res.second);
            }
        }
    }
}
} // namespace dcscm_fru
