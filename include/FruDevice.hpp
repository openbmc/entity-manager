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

int busStrToInt(const std::string& busName);

std::set<int> findI2CEeproms(int i2cBus,
                             const std::shared_ptr<DeviceMap>& devices);

int getBusFRUs(int file, int first, int last, int bus,
               std::shared_ptr<DeviceMap> devices,
               sdbusplus::asio::object_server& objServer);

void loadBlacklist(const char* path);

bool writeFRU(uint8_t bus, uint8_t address, const std::vector<uint8_t>& fru);

void rescanOneBus(
    BusMap& busmap, uint8_t busNum,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    bool dbusCall, sdbusplus::asio::object_server& objServer,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus);

void rescanBusses(
    BusMap& busmap,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    sdbusplus::asio::object_server& objServer,
    std::shared_ptr<sdbusplus::asio::connection> systemBus);
