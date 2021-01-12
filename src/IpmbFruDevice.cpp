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
/// \file IpmbFruDevice.cpp

#include "IpmbFruDevice.hpp"

#include "FruUtils.hpp"
#include "Utils.hpp"

#include <VariantVisitors.hpp>

using ManagedObjectType = boost::container::flat_map<
    sdbusplus::message::object_path,
    boost::container::flat_map<
        std::string,
        boost::container::flat_map<std::string, BasicVariantType>>>;

std::vector<uint8_t> ipmbAddress;

IpmbMethodType
    dbusNewMethodCall(uint8_t ipmbAddress, uint8_t netFn, uint8_t lun,
                      uint8_t cmd, std::vector<uint8_t> cmdData,
                      std::shared_ptr<sdbusplus::asio::connection>& systemBus)
{
    IpmbMethodType resp;
    auto method =
        systemBus->new_method_call("xyz.openbmc_project.Ipmi.Channel.Ipmb",
                                   "/xyz/openbmc_project/Ipmi/Channel/Ipmb",
                                   "org.openbmc.Ipmb", "sendRequest");

    method.append(ipmbAddress, netFn, lun, cmd, cmdData);

    auto reply = systemBus->call(method);
    if (reply.is_method_error())
    {
        std::cerr << "Error reading from Ipmb";
    }

    reply.read(resp);
    return resp;
}

void ipmbWrite(uint8_t ipmbAddress, uint8_t offset,
               const std::vector<uint8_t>& fru,
               std::shared_ptr<sdbusplus::asio::connection> systemBus)
{
    uint8_t cmd = 0x12;
    uint8_t netFn = 0xa;
    uint8_t lun = 0;
    std::vector<uint8_t> cmdData;
    std::vector<uint8_t> respData;

    cmdData.emplace_back(fru.size() + 3);
    cmdData.emplace_back(0);
    cmdData.emplace_back(offset);
    cmdData.emplace_back(0);
    cmdData.emplace_back(fru.size());

    for (uint8_t iter = 0; iter < fru.size(); iter++)
    {
        cmdData.emplace_back(fru[iter]);
    }

    IpmbMethodType resp =
        dbusNewMethodCall(ipmbAddress, netFn, lun, cmd, cmdData, systemBus);

    respData =
        std::move(std::get<std::remove_reference_t<decltype(respData)>>(resp));

    return;
}

bool ipmbWriteFru(uint8_t bus, const std::vector<uint8_t>& fru,
                  std::shared_ptr<sdbusplus::asio::connection> systemBus)
{
    uint8_t offset = 0;

    boost::container::flat_map<std::string, std::string> tmp;
    if (fru.size() > maxFruSize)
    {
        std::cerr << "Invalid fru.size() during writeFru\n";
        return false;
    }

    // verify legal fru by running it through fru parsing logic
    if (formatFRU(fru, tmp) != resCodes::resOK)
    {
        std::cerr << "Invalid fru format during writeFRU\n";
        return false;
    }

    ipmbWrite(ipmbAddress[bus], offset, fru, systemBus);

    return true;
}

void findIpmbDev(std::vector<uint8_t>& ipmbAddr,
                 std::shared_ptr<sdbusplus::asio::connection>& conn)
{
    uint8_t devIndex = 0;
    uint8_t type = 0;
    std::string index;
    std::string typeConfig;

    if (!conn)
    {
        std::cerr << "Connection failed \n";
        return;
    }

    auto method = conn->new_method_call(
        "xyz.openbmc_project.EntityManager", "/",
        "org.freedesktop.DBus.ObjectManager", "GetManagedObjects");

    method.append();
    auto reply = conn->call(method);
    if (reply.is_method_error())
    {
        std::cerr << "Error contacting entity manager in find objects \n";
    }

    ManagedObjectType resp;
    reply.read(resp);
    for (const auto& pathPair : resp)
    {
        for (const auto& entry : pathPair.second)
        {
            if (entry.first != configInterface)
            {
                continue;
            }

            index = loadVariant<std::string>(entry.second, "Index");
            typeConfig = loadVariant<std::string>(entry.second, "Bus");

            devIndex = atoi(index.c_str());

            if (typeConfig.compare("me") == 0)
            {
                type = 1;
            }
            else if (typeConfig.compare("ipmb") == 0)
            {
                type = 0;
            }
            else
            {
                std::cerr << "findIpmbDev : Invalid type in json\n";
                return;
            }

            ipmbAddr.push_back(((devIndex << 2) | type));
        }
    }
}

void getAreaInfo(uint8_t ipmbAddress, uint8_t& compCode, uint16_t& readLen,
                 std::shared_ptr<sdbusplus::asio::connection>& systemBus)
{
    // Command for get the FRU Area Info
    uint8_t cmd = 0x10;

    // Netfunction for get the FRU Area Info
    uint8_t netFn = 0xa;

    static const uint8_t lun = 0;

    std::vector<uint8_t> cmdData{0};
    std::vector<uint8_t> respData;

    IpmbMethodType resp =
        dbusNewMethodCall(ipmbAddress, netFn, lun, cmd, cmdData, systemBus);

    // Assign Response completion code
    compCode = std::get<4>(resp);

    std::vector<uint8_t> data;
    data = std::get<5>(resp);

    if (data.size() < 2)
    {
        std::cerr << "Data size is less then 2." << std::endl;
        return;
    }

    readLen = ((data[1] << 8) | data[0]);
}

void ipmbReadFru(uint8_t ipmbAddress, uint8_t offset,
                 std::vector<uint8_t>& fruResponse,
                 std::shared_ptr<sdbusplus::asio::connection>& systemBus)
{
    uint8_t cmd = 0x11;
    uint8_t netFn = 0xa;
    uint8_t lun = 0;
    std::vector<uint8_t> cmdData;
    std::vector<uint8_t> respData;

    cmdData.emplace_back(0);
    cmdData.emplace_back(offset);
    cmdData.emplace_back(0);
    cmdData.emplace_back(49);

    IpmbMethodType resp =
        dbusNewMethodCall(ipmbAddress, netFn, lun, cmd, cmdData, systemBus);

    respData =
        std::move(std::get<std::remove_reference_t<decltype(respData)>>(resp));

    respData.erase(respData.begin());

    copy(respData.begin(), respData.end(), back_inserter(fruResponse));
}

void ipmbScanDevices(
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    sdbusplus::asio::object_server& objServer)
{
    std::vector<uint8_t> fruResponse;
    boost::container::flat_map<int, std::vector<uint8_t>> hostDev;

    uint8_t compCode = 0;
    uint8_t iter = 0;
    uint16_t readLen = 0;
    int offset = 0;
    int count = 0;

    hostDev.clear();
    busMap.clear();

    for (auto& busIface : dbusInterfaceMap)
    {
        objServer.remove_interface(busIface.second);
    }

    dbusInterfaceMap.clear();

    ipmbAddress.clear();

    findIpmbDev(ipmbAddress, systemBus);

    for (iter = 0; iter < ipmbAddress.size(); iter++)
    {
        // Add code for Get FRU Area Info
        getAreaInfo(ipmbAddress[iter], compCode, readLen, systemBus);

        if (compCode == 0)
        {
            while (offset < readLen)
            {
                ipmbReadFru(ipmbAddress[iter], offset, fruResponse, systemBus);
                offset = offset + 49;
            }
        }

        hostDev[0] = fruResponse;
        busMap[count++] = std::make_shared<DeviceMap>(hostDev);

        offset = 0;
        readLen = 0;
        fruResponse.clear();
    }

    for (auto& devicemap : busMap)
    {
        for (auto& device : *devicemap.second)
        {
            addFruObjectToDbus(device.second, dbusInterfaceMap, devicemap.first,
                               device.first, objServer, true, systemBus);
        }
    }
}
