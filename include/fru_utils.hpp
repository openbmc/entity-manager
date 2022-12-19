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
/// \file fru_utils.hpp

#pragma once
#include "fru_reader.hpp"

#include <boost/asio/io_service.hpp>
#include <boost/container/flat_map.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <cstdint>
#include <functional>
#include <regex>
#include <string>
#include <utility>
#include <vector>
extern "C"
{
// Include for I2C_SMBUS_BLOCK_MAX
#include <linux/i2c.h>
}

constexpr size_t fruBlockSize = 8;

using DeviceMap = boost::container::flat_map<int, std::vector<uint8_t>>;
using BusMap = boost::container::flat_map<int, std::shared_ptr<DeviceMap>>;

inline BusMap busMap;

enum class DecodeState
{
    ok,
    end,
    err,
};

enum class resCodes
{
    resOK,
    resWarn,
    resErr
};

enum class fruAreas
{
    fruAreaInternal = 0,
    fruAreaChassis,
    fruAreaBoard,
    fruAreaProduct,
    fruAreaMultirecord
};

struct FruArea
{
    size_t start;          // Fru Area Start offset
    size_t size;           // Fru Area Size
    size_t end;            // Fru Area end offset
    size_t updateFieldLoc; // Fru Area update Field Location
    size_t restFieldsLoc;  // Starting location of restFRUArea data
    size_t restFieldsEnd;  // Ending location of restFRUArea data
};

const std::vector<std::string> fruAreaNames = {"INTERNAL", "CHASSIS", "BOARD",
                                               "PRODUCT", "MULTIRECORD"};
const std::regex nonAsciiRegex("[^\x01-\x7f]");

const std::vector<std::string> chassisFruAreas = {"PART_NUMBER",
                                                  "SERIAL_NUMBER"};

const std::vector<std::string> boardFruAreas = {"MANUFACTURER", "PRODUCT_NAME",
                                                "SERIAL_NUMBER", "PART_NUMBER",
                                                "FRU_VERSION_ID"};

const std::vector<std::string> productFruAreas = {
    "MANUFACTURER",  "PRODUCT_NAME", "PART_NUMBER",   "VERSION",
    "SERIAL_NUMBER", "ASSET_TAG",    "FRU_VERSION_ID"};

const std::string fruCustomFieldName = "INFO_AM";

inline fruAreas operator++(fruAreas& x)
{
    return x = static_cast<fruAreas>(std::underlying_type<fruAreas>::type(x) +
                                     1);
}

inline const std::string& getFruAreaName(fruAreas area)
{
    return fruAreaNames[static_cast<unsigned int>(area)];
}

std::tm intelEpoch(void);

char sixBitToChar(uint8_t val);

/* 0xd - 0xf are reserved values, but not fatal; use a placeholder char. */
constexpr std::array<char, 6> bcdHighChars = {
    ' ', '-', '.', 'X', 'X', 'X',
};

char bcdPlusToChar(uint8_t val);

bool verifyOffset(const std::vector<uint8_t>& fruBytes, fruAreas currentArea,
                  uint8_t len);

std::pair<DecodeState, std::string>
    decodeFRUData(std::vector<uint8_t>::const_iterator& iter,
                  const std::vector<uint8_t>::const_iterator& end,
                  bool isLangEng);

bool checkLangEng(uint8_t lang);

resCodes
    formatIPMIFRU(const std::vector<uint8_t>& fruBytes,
                  boost::container::flat_map<std::string, std::string>& result);

std::vector<uint8_t>& getFRUInfo(const uint16_t& bus, const uint8_t& address);

uint8_t calculateChecksum(std::vector<uint8_t>::const_iterator iter,
                          std::vector<uint8_t>::const_iterator end);

uint8_t calculateChecksum(std::vector<uint8_t>& fruAreaData);

unsigned int updateFRUAreaLenAndChecksum(std::vector<uint8_t>& fruData,
                                         size_t fruAreaStart,
                                         size_t fruAreaEndOfFieldsOffset,
                                         size_t fruAreaEndOffset);

ssize_t getFieldLength(uint8_t fruFieldTypeLenValue);

/// \brief Find a FRU header.
/// \param reader the FRUReader to read via
/// \param errorHelp and a helper string for failures
/// \param blockData buffer to return the last read block
/// \param baseOffset the offset to start the search at;
///        set to 0 to perform search;
///        returns the offset at which a header was found
/// \return whether a header was found
bool findFRUHeader(FRUReader& reader, const std::string& errorHelp,
                   std::array<uint8_t, I2C_SMBUS_BLOCK_MAX>& blockData,
                   off_t& baseOffset);

/// \brief Read and validate FRU contents.
/// \param reader the FRUReader to read via
/// \param errorHelp and a helper string for failures
/// \return the FRU contents from the file
std::vector<uint8_t> readFRUContents(FRUReader& reader,
                                     const std::string& errorHelp);

/// \brief Validate an IPMI FRU common header
/// \param blockData the bytes comprising the common header
/// \return true if valid
bool validateHeader(const std::array<uint8_t, I2C_SMBUS_BLOCK_MAX>& blockData);

/// \brief Get offset for a common header area
/// \param area - the area
/// \return the field offset
unsigned int getHeaderAreaFieldOffset(fruAreas area);

/// \brief Iterate fruArea Names and find offset/location and fields and size of
/// properties
/// \param fruData - vector to store fru data
/// \param propertyName - fru property Name
/// \param fruAreaParams - struct to have fru Area parameters like length,
/// size.
/// \return true if fru field is found, fruAreaParams like updateFieldLoc,
/// Start, Size, End are updated with fruArea and field info.
bool findFruAreaLocationAndField(std::vector<uint8_t>& fruData,
                                 const std::string& propertyName,
                                 struct FruArea& fruAreaParams);

/// \brief Copy the fru Area fields and properties into restFRUAreaFieldsData.
/// restFRUAreaField is the rest of the fields in FRU area after the field that
/// is being updated.
/// \param fruData - vector to store fru data
/// \param propertyName - fru property Name
/// \param fruAreaParams - struct to have fru Area parameters like length
/// \param restFRUAreaFieldsData - vector to store fru Area Fields and
/// properties.
/// \return true on success false on failure. restFieldLoc and restFieldEnd
/// are updated.
bool copyRestFRUArea(std::vector<uint8_t>& fruData,
                     const std::string& propertyName,
                     struct FruArea& fruAreaParams,
                     std::vector<uint8_t>& restFRUAreaFieldsData);

/// \brief Scan the I2c and Ipmb busses and get the fru device details.
/// \param busmap - Map of bus and fru devices.
/// \param dbusInterfaceMap - Map to store fru device and dbus objects.
/// \param unknownBusObjectCount - count to store unknown bus objects
/// \param powerIsOn - bool variable to check whether power On or Off.
/// \param objServer - sdbusplus asio object server for dbus objects
/// \param systemBus - sdbusplus asio connection for dbus service
/// \return void.
void rescanBusses(
    BusMap& busmap,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    size_t& unknownBusObjectCount, const bool& powerIsOn,
    sdbusplus::asio::object_server& objServer,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus);

/// \brief It does format fru data and find productName in the formatted
/// fru data and return productName.
/// \param device - vector that contains device list
/// \param formattedFRU - map that contains formatted FRU data
/// \param bus - bus number of the device
/// \param address - address of the device
/// \param unknownBusObjectCount - Unknown Bus object counter variable
/// \return optional string. it returns productName or NULL
std::optional<std::string> getProductName(
    std::vector<uint8_t>& device,
    boost::container::flat_map<std::string, std::string>& formattedFRU,
    uint32_t bus, uint32_t address, size_t& unknownBusObjectCount);

bool getFruData(std::vector<uint8_t>& fruData, uint32_t bus, uint32_t address);

/// \brief Find location of product area info, offset and update fru properties
/// like manufacturer name, product name, product version, & product
/// serial number and Asset tag.
/// \param updatePropertyReq - string to store update property request.
/// \param bus - I2C bus number of fru device.
/// \param address - I2C bus address of bus for fru device.
/// \param propertyName - string to store property name.
/// \param dbusInterfaceMap - Map to store fru device and dbus objects.
/// \param unknownBusObjectCount - count to store unknown bus objects
/// \param powerIsOn - bool variable to check whether power On or Off.
/// \param objServer - sdbusplus asio object server for dbus objects
/// \param systemBus - sdbusplus asio connection for dbus service
/// \return true on success false on failure.
bool updateFRUProperty(
    const std::string& updatePropertyReq, uint32_t bus, uint32_t address,
    const std::string& propertyName,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    size_t& unknownBusObjectCount, const bool& powerIsOn,
    sdbusplus::asio::object_server& objServer,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus);
