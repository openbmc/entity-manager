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

#include "FruUtils.hpp"
#include "Utils.hpp"

using DeviceMap = boost::container::flat_map<int, std::vector<uint8_t>>;
using BusMap = boost::container::flat_map<int, std::shared_ptr<DeviceMap>>;

extern BusMap busMap;

extern bool powerIsOn;

constexpr const char* configInterface =
    "xyz.openbmc_project.Configuration.IPMIChannels";

using IpmbMethodType =
    std::tuple<int, uint8_t, uint8_t, uint8_t, uint8_t, std::vector<uint8_t>>;

IpmbMethodType
    dbusNewMethodCall(uint8_t ipmbAddress, uint8_t netFn, uint8_t lun,
                      uint8_t cmd, std::vector<uint8_t> cmdData,
                      std::shared_ptr<sdbusplus::asio::connection>& systemBus);

void ipmbWrite(uint8_t ipmbAddress, uint8_t offset,
               const std::vector<uint8_t>& fru,
               std::shared_ptr<sdbusplus::asio::connection> systemBus);

bool ipmbWriteFru(uint8_t bus, const std::vector<uint8_t>& fru,
                  std::shared_ptr<sdbusplus::asio::connection> systemBus);

void findIpmbDev(std::vector<uint8_t>& ipmbAddr,
                 std::shared_ptr<sdbusplus::asio::connection>& conn);

void getAreaInfo(uint8_t ipmbAddress, uint8_t& compCode, uint16_t& readLen,
                 std::shared_ptr<sdbusplus::asio::connection>& systemBus);

void ipmbReadFru(uint8_t ipmbAddress, uint8_t offset,
                 std::vector<uint8_t>& fruResponse,
                 std::shared_ptr<sdbusplus::asio::connection>& systemBus);

void ipmbScanDevices(
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    sdbusplus::asio::object_server& objServer);
