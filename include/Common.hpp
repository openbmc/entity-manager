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
/// \file EntityManager.hpp

#pragma once

#include "Utils.hpp"

#include <systemd/sd-journal.h>

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>
#include <list>
#include <regex>
#include <string>

constexpr size_t fruBlockSize = 8;

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

const std::vector<std::string> FRU_AREA_NAMES = {"INTERNAL", "CHASSIS", "BOARD",
                                                 "PRODUCT", "MULTIRECORD"};
const std::regex NON_ASCII_REGEX("[^\x01-\x7f]");

const std::vector<std::string> CHASSIS_FRU_AREAS = {"PART_NUMBER",
                                                    "SERIAL_NUMBER"};

const std::vector<std::string> BOARD_FRU_AREAS = {
    "MANUFACTURER", "PRODUCT_NAME", "SERIAL_NUMBER", "PART_NUMBER",
    "FRU_VERSION_ID"};

const std::vector<std::string> PRODUCT_FRU_AREAS = {
    "MANUFACTURER",  "PRODUCT_NAME", "PART_NUMBER",   "VERSION",
    "SERIAL_NUMBER", "ASSET_TAG",    "FRU_VERSION_ID"};

const std::string FRU_CUSTOM_FIELD_NAME = "INFO_AM";

inline fruAreas operator++(fruAreas& x)
{
    return x = static_cast<fruAreas>(std::underlying_type<fruAreas>::type(x) +
                                     1);
}

inline unsigned int getHeaderAreaFieldOffset(fruAreas area)
{
    return static_cast<unsigned int>(area) + 1;
}

inline const std::string& getFruAreaName(fruAreas area)
{
    return FRU_AREA_NAMES[static_cast<unsigned int>(area)];
}

bool isMuxBus(size_t bus);

const std::tm intelEpoch(void);

char sixBitToChar(uint8_t val);

/* 0xd - 0xf are reserved values, but not fatal; use a placeholder char. */
const char bcdHighChars[] = {
    ' ', '-', '.', 'X', 'X', 'X',
};

char bcdPlusToChar(uint8_t val);

std::pair<DecodeState, std::string>
    decodeFRUData(std::vector<uint8_t>::const_iterator& iter,
                  const std::vector<uint8_t>::const_iterator& end);

void checkLang(uint8_t lang);

bool formatFRU(const std::vector<uint8_t>& fruBytes,
               boost::container::flat_map<std::string, std::string>& result);

std::vector<uint8_t>& getFRUInfo(const uint8_t& bus, const uint8_t& address);

uint8_t calculateChecksum(std::vector<uint8_t>::const_iterator iter,
                          std::vector<uint8_t>::const_iterator end);

uint8_t calculateChecksum(std::vector<uint8_t>& fruAreaData);

unsigned int updateFRUAreaLenAndChecksum(std::vector<uint8_t>& fruData,
                                         size_t fruAreaStart,
                                         size_t fruAreaEndOfFieldsOffset,
                                         size_t fruAreaEndOffset);

ssize_t getFieldLength(uint8_t fruFieldTypeLenValue);
