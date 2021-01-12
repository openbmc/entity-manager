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

#include "FruUtils.hpp"
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

extern "C"
{
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
}

std::vector<uint8_t> ipmbAddress;

using IpmbMethodType =
    std::tuple<int, uint8_t, uint8_t, uint8_t, uint8_t, std::vector<uint8_t>>;

static constexpr bool DEBUG = false;
static size_t UNKNOWN_BUS_OBJECT_COUNT = 0;
constexpr size_t MAX_FRU_SIZE = 1024;

using DeviceMap = boost::container::flat_map<int, std::vector<uint8_t>>;
using BusMap = boost::container::flat_map<int, std::shared_ptr<DeviceMap>>;

static BusMap busMap;

boost::asio::io_service io;
auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
auto objServer = sdbusplus::asio::object_server(systemBus);

bool updateFRUProperty(
    const std::string& assetTag, uint32_t bus, uint32_t address,
    std::string propertyName,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap);

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
    return device->second;
}

void AddFRUObjectToDbus(
    std::vector<uint8_t>& device,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    uint32_t bus, uint32_t address)
{
    boost::container::flat_map<std::string, std::string> formattedFRU;
    resCodes res = formatFRU(device, formattedFRU);
    if (res == resCodes::resErr)
    {
        std::cerr << "failed to parse FRU for device at bus " << bus
                  << " address " << address << "\n";
        return;
    }
    else if (res == resCodes::resWarn)
    {
        std::cerr << "there were warnings while parsing FRU for device at bus "
                  << bus << " address " << address << "\n";
    }

    auto productNameFind = formattedFRU.find("BOARD_PRODUCT_NAME");
    std::string productName;
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
        productName = "UNKNOWN" + std::to_string(UNKNOWN_BUS_OBJECT_COUNT);
        UNKNOWN_BUS_OBJECT_COUNT++;
    }

    if (bus == 0)
    {
        productName =
            "/xyz/openbmc_project/Ipmb/FruDevice/" + productName + "_0";
    }
    // avoid duplicates by checking to see if on a mux
    if (bus > 0)
    {
        int highest = 0;
        bool found = false;

        productName = "/xyz/openbmc_project/Ipmb/FruDevice/" + productName;
        for (auto const& busIface : dbusInterfaceMap)
        {
            std::string path = busIface.second->get_object_path();
            if (std::regex_match(path, std::regex(productName + "(_\\d+|)$")))
            {
                if ((getFRUInfo(static_cast<uint8_t>(busIface.first.first),
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
                std::smatch base_match;

                bool match = std::regex_match(
                    path, base_match, std::regex(productName + "_(\\d+)$"));
                if (match)
                {
                    if (base_match.size() == 2)
                    {
                        std::ssub_match base_sub_match = base_match[1];
                        std::string base = base_sub_match.str();

                        int value = std::stoi(base);
                        highest = (value > highest) ? value : highest;
                    }
                }
            }
        } // end searching objects

        if (found)
        {
            // We found something with the same name.  If highest was still 0,
            // it means this new entry will be _1.
            productName += "_";
            productName += std::to_string(++highest);
        }
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface(productName,
                                "xyz.openbmc_project.Ipmb.FruDevice");
    dbusInterfaceMap[std::pair<size_t, size_t>(bus, address)] = iface;

    for (auto& property : formattedFRU)
    {
        std::regex_replace(property.second.begin(), property.second.begin(),
                           property.second.end(), NON_ASCII_REGEX, "_");
        if (property.second.empty() && property.first != "PRODUCT_ASSET_TAG")
        {
            continue;
        }
        std::string key =
            std::regex_replace(property.first, NON_ASCII_REGEX, "_");

        if (property.first == "PRODUCT_ASSET_TAG")
        {
            std::string propertyName = property.first;
            iface->register_property(
                key, property.second + '\0',
                [bus, address, propertyName,
                 &dbusInterfaceMap](const std::string& req, std::string& resp) {
                    if (strcmp(req.c_str(), resp.c_str()))
                    {
                        // call the method which will update
                        if (updateFRUProperty(req, bus, address, propertyName,
                                              dbusInterfaceMap))
                        {
                            resp = req;
                        }
                        else
                        {
                            throw std::invalid_argument(
                                "FRU property update failed.");
                        }
                    }
                    return 1;
                });
        }
        else if (!iface->register_property(key, property.second + '\0'))
        {
            std::cerr << "illegal key: " << key << "\n";
        }
        if (DEBUG)
        {
            std::cout << property.first << ": " << property.second << "\n";
        }
    }

    // baseboard will be 0, 0
    iface->register_property("BUS", bus);
    iface->register_property("ADDRESS", address);

    iface->initialize();
}

IpmbMethodType dbusNewMethodCall(uint8_t ipmbAddress, uint8_t netFn,
                                 uint8_t lun, uint8_t cmd,
                                 std::vector<uint8_t> cmdData)
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
               const std::vector<uint8_t>& fru)
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
        dbusNewMethodCall(ipmbAddress, netFn, lun, cmd, cmdData);

    respData =
        std::move(std::get<std::remove_reference_t<decltype(respData)>>(resp));

    return;
}

bool ipmbWriteFru(uint8_t bus, const std::vector<uint8_t>& fru)
{
    uint8_t offset = 0;

    boost::container::flat_map<std::string, std::string> tmp;
    if (fru.size() > MAX_FRU_SIZE)
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

    ipmbWrite(ipmbAddress[bus], offset, fru);

    return true;
}

void findIpmbDev(std::vector<uint8_t>& ipmbAddr)
{
    constexpr const char* configFilePath =
        "/usr/share/entity-manager/configurations/FBYV2.json";

    uint8_t devIndex = 0;
    uint8_t type = 0;

    std::ifstream configFile(configFilePath);
    if (!configFile.is_open())
    {
        std::cerr << "findIpmbDev : Cannot open config path \n";
        return;
    }
    try
    {
        auto data = nlohmann::json::parse(configFile, nullptr);
        for (const auto& channelConfig : data["Ipmb_channels"])
        {
            const std::string& typeConfig = channelConfig["type"];
            devIndex = (static_cast<uint8_t>(channelConfig["devIndex"]));

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
    catch (nlohmann::json::exception& e)
    {
        std::cerr << "findIpmbDev : Error parsing config file \n";
        return;
    }
    catch (std::out_of_range& e)
    {
        std::cerr << "findIpmbDev : Error invalid type \n";
        return;
    }
}

void getAreaInfo(uint8_t ipmbAddress, uint8_t& compCode, uint16_t& readLen)
{
    // Command for get the FRU Area Info
    uint8_t cmd = 0x10;

    // Netfunction for get the FRU Area Info
    uint8_t netFn = 0xa;

    static const uint8_t lun = 0;

    std::vector<uint8_t> cmdData{0};
    std::vector<uint8_t> respData;

    IpmbMethodType resp =
        dbusNewMethodCall(ipmbAddress, netFn, lun, cmd, cmdData);

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
                 std::vector<uint8_t>& fruResponse)
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
        dbusNewMethodCall(ipmbAddress, netFn, lun, cmd, cmdData);

    respData =
        std::move(std::get<std::remove_reference_t<decltype(respData)>>(resp));

    respData.erase(respData.begin());

    copy(respData.begin(), respData.end(), back_inserter(fruResponse));
}

void ipmbScanDevices(
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap)
{
    std::vector<uint8_t> fruResponse;
    boost::container::flat_map<int, std::vector<uint8_t>> hostDev;

    uint8_t compCode = 0;
    uint16_t readLen = 0;
    uint8_t iter = 0;
    int offset = 0;
    int count = 0;

    hostDev.clear();
    busMap.clear();

    for (auto& busIface : dbusInterfaceMap)
    {
        objServer.remove_interface(busIface.second);
    }

    dbusInterfaceMap.clear();
    UNKNOWN_BUS_OBJECT_COUNT = 0;

    ipmbAddress.clear();
    findIpmbDev(ipmbAddress);

    for (iter = 0; iter < ipmbAddress.size(); iter++)
    {
        // Add code for Get FRU Area Info
        getAreaInfo(ipmbAddress[iter], compCode, readLen);

        if (compCode == 0)
        {
            while (offset < readLen)
            {
                ipmbReadFru(ipmbAddress[iter], offset, fruResponse);
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
            AddFRUObjectToDbus(device.second, dbusInterfaceMap, devicemap.first,
                               device.first);
        }
    }
}

// Details with example of Asset Tag Update
// To find location of Product Info Area asset tag as per FRU specification
// 1. Find product Info area starting offset (*8 - as header will be in
// multiple of 8 bytes).
// 2. Skip 3 bytes of product info area (like format version, area length,
// and language code).
// 3. Traverse manufacturer name, product name, product version, & product
// serial number, by reading type/length code to reach the Asset Tag.
// 4. Update the Asset Tag, reposition the product Info area in multiple of
// 8 bytes. Update the Product area length and checksum.

bool updateFRUProperty(
    const std::string& updatePropertyReq, uint32_t bus, uint32_t address,
    std::string propertyName,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap)
{
    size_t updatePropertyReqLen = updatePropertyReq.length();
    if (updatePropertyReqLen == 1 || updatePropertyReqLen > 63)
    {
        std::cerr
            << "FRU field data cannot be of 1 char or more than 63 chars. "
               "Invalid Length "
            << updatePropertyReqLen << "\n";
        return false;
    }

    std::vector<uint8_t> fruData;
    try
    {
        fruData = getFRUInfo(static_cast<uint8_t>(bus),
                             static_cast<uint8_t>(address));
    }
    catch (std::invalid_argument& e)
    {
        std::cerr << "Failure getting FRU Info" << e.what() << "\n";
        return false;
    }

    if (fruData.empty())
    {
        return false;
    }

    const std::vector<std::string>* fruAreaFieldNames;

    uint8_t fruAreaOffsetFieldValue = 0;
    size_t offset = 0;
    std::string areaName = propertyName.substr(0, propertyName.find("_"));
    std::string propertyNamePrefix = areaName + "_";
    auto it = std::find(FRU_AREA_NAMES.begin(), FRU_AREA_NAMES.end(), areaName);
    if (it == FRU_AREA_NAMES.end())
    {
        std::cerr << "Can't parse area name for property " << propertyName
                  << " \n";
        return false;
    }
    fruAreas fruAreaToUpdate =
        static_cast<fruAreas>(it - FRU_AREA_NAMES.begin());
    fruAreaOffsetFieldValue =
        fruData[getHeaderAreaFieldOffset(fruAreaToUpdate)];
    switch (fruAreaToUpdate)
    {
        case fruAreas::fruAreaChassis:
            offset = 3; // chassis part number offset. Skip fixed first 3 bytes
            fruAreaFieldNames = &CHASSIS_FRU_AREAS;
            break;
        case fruAreas::fruAreaBoard:
            offset = 6; // board manufacturer offset. Skip fixed first 6 bytes
            fruAreaFieldNames = &BOARD_FRU_AREAS;
            break;
        case fruAreas::fruAreaProduct:
            // Manufacturer name offset. Skip fixed first 3 product fru bytes
            // i.e. version, area length and language code
            offset = 3;
            fruAreaFieldNames = &PRODUCT_FRU_AREAS;
            break;
        default:
            std::cerr << "Don't know how to handle property " << propertyName
                      << " \n";
            return false;
    }
    if (fruAreaOffsetFieldValue == 0)
    {
        std::cerr << "FRU Area for " << propertyName << " not present \n";
        return false;
    }

    size_t fruAreaStart = fruAreaOffsetFieldValue * fruBlockSize;
    size_t fruAreaSize = fruData[fruAreaStart + 1] * fruBlockSize;
    size_t fruAreaEnd = fruAreaStart + fruAreaSize;
    size_t fruDataIter = fruAreaStart + offset;
    size_t fruUpdateFieldLoc = fruDataIter;
    size_t skipToFRUUpdateField = 0;
    ssize_t fieldLength;

    bool found = false;
    for (auto& field : *fruAreaFieldNames)
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
        std::size_t pos = propertyName.find(FRU_CUSTOM_FIELD_NAME);
        if (pos == std::string::npos)
        {
            std::cerr << "PropertyName doesn't exist in FRU Area Vectors: "
                      << propertyName << "\n";
            return false;
        }
        std::string fieldNumStr =
            propertyName.substr(pos + FRU_CUSTOM_FIELD_NAME.length());
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
        fieldLength = getFieldLength(fruData[fruDataIter]);
        if (fieldLength < 0)
        {
            break;
        }
        fruDataIter += 1 + fieldLength;
    }
    fruUpdateFieldLoc = fruDataIter;

    // Push post update fru field bytes to a vector
    fieldLength = getFieldLength(fruData[fruUpdateFieldLoc]);
    if (fieldLength < 0)
    {
        std::cerr << "Property " << propertyName << " not present \n";
        return false;
    }
    fruDataIter += 1 + fieldLength;
    size_t restFRUFieldsLoc = fruDataIter;
    size_t endOfFieldsLoc = 0;
    while ((fieldLength = getFieldLength(fruData[fruDataIter])) >= 0)
    {
        if (fruDataIter >= (fruAreaStart + fruAreaSize))
        {
            fruDataIter = fruAreaStart + fruAreaSize;
            break;
        }
        fruDataIter += 1 + fieldLength;
    }
    endOfFieldsLoc = fruDataIter;

    std::vector<uint8_t> restFRUAreaFieldsData;
    std::copy_n(fruData.begin() + restFRUFieldsLoc,
                endOfFieldsLoc - restFRUFieldsLoc + 1,
                std::back_inserter(restFRUAreaFieldsData));

    // Push post update fru areas if any
    unsigned int nextFRUAreaLoc = 0;
    for (fruAreas nextFRUArea = fruAreas::fruAreaInternal;
         nextFRUArea <= fruAreas::fruAreaMultirecord; ++nextFRUArea)
    {
        unsigned int fruAreaLoc =
            fruData[getHeaderAreaFieldOffset(nextFRUArea)] * fruBlockSize;
        if ((fruAreaLoc > endOfFieldsLoc) &&
            ((nextFRUAreaLoc == 0) || (fruAreaLoc < nextFRUAreaLoc)))
        {
            nextFRUAreaLoc = fruAreaLoc;
        }
    }
    std::vector<uint8_t> restFRUAreasData;
    if (nextFRUAreaLoc)
    {
        std::copy_n(fruData.begin() + nextFRUAreaLoc,
                    fruData.size() - nextFRUAreaLoc,
                    std::back_inserter(restFRUAreasData));
    }

    // check FRU area size
    size_t fruAreaDataSize =
        ((fruUpdateFieldLoc - fruAreaStart + 1) + restFRUAreaFieldsData.size());
    size_t fruAreaAvailableSize = fruAreaSize - fruAreaDataSize;
    if ((updatePropertyReqLen + 1) > fruAreaAvailableSize)
    {

#ifdef ENABLE_FRU_AREA_RESIZE
        size_t newFRUAreaSize = fruAreaDataSize + updatePropertyReqLen + 1;
        // round size to 8-byte blocks
        newFRUAreaSize =
            ((newFRUAreaSize - 1) / fruBlockSize + 1) * fruBlockSize;
        size_t newFRUDataSize = fruData.size() + newFRUAreaSize - fruAreaSize;
        fruData.resize(newFRUDataSize);
        fruAreaSize = newFRUAreaSize;
        fruAreaEnd = fruAreaStart + fruAreaSize;
#else
        std::cerr << "FRU field length: " << updatePropertyReqLen + 1
                  << " should not be greater than available FRU area size: "
                  << fruAreaAvailableSize << "\n";
        return false;
#endif // ENABLE_FRU_AREA_RESIZE
    }

    // write new requested property field length and data
    constexpr uint8_t newTypeLenMask = 0xC0;
    fruData[fruUpdateFieldLoc] =
        static_cast<uint8_t>(updatePropertyReqLen | newTypeLenMask);
    fruUpdateFieldLoc++;
    std::copy(updatePropertyReq.begin(), updatePropertyReq.end(),
              fruData.begin() + fruUpdateFieldLoc);

    // Copy remaining data to main fru area - post updated fru field vector
    restFRUFieldsLoc = fruUpdateFieldLoc + updatePropertyReqLen;
    size_t fruAreaDataEnd = restFRUFieldsLoc + restFRUAreaFieldsData.size();
    std::copy(restFRUAreaFieldsData.begin(), restFRUAreaFieldsData.end(),
              fruData.begin() + restFRUFieldsLoc);

    // Update final fru with new fru area length and checksum
    unsigned int nextFRUAreaNewLoc = updateFRUAreaLenAndChecksum(
        fruData, fruAreaStart, fruAreaDataEnd, fruAreaEnd);

#ifdef ENABLE_FRU_AREA_RESIZE
    ++nextFRUAreaNewLoc;
    ssize_t nextFRUAreaOffsetDiff =
        (nextFRUAreaNewLoc - nextFRUAreaLoc) / fruBlockSize;
    // Append rest FRU Areas if size changed and there were other sections after
    // updated one
    if (nextFRUAreaOffsetDiff && nextFRUAreaLoc)
    {
        std::copy(restFRUAreasData.begin(), restFRUAreasData.end(),
                  fruData.begin() + nextFRUAreaNewLoc);
        // Update Common Header
        for (int fruArea = fruAreaInternal; fruArea <= fruAreaMultirecord;
             fruArea++)
        {
            unsigned int fruAreaOffsetField = getHeaderAreaFieldOffset(fruArea);
            size_t curFRUAreaOffset = fruData[fruAreaOffsetField];
            if (curFRUAreaOffset > fruAreaOffsetFieldValue)
            {
                fruData[fruAreaOffsetField] = static_cast<int8_t>(
                    curFRUAreaOffset + nextFRUAreaOffsetDiff);
            }
        }
        // Calculate new checksum
        std::vector<uint8_t> headerFRUData;
        std::copy_n(fruData.begin(), 7, std::back_inserter(headerFRUData));
        size_t checksumVal = calculateChecksum(headerFRUData);
        fruData[7] = static_cast<uint8_t>(checksumVal);
        // fill zeros if FRU Area size decreased
        if (nextFRUAreaOffsetDiff < 0)
        {
            std::fill(fruData.begin() + nextFRUAreaNewLoc +
                          restFRUAreasData.size(),
                      fruData.end(), 0);
        }
    }
#else
    // this is to avoid "unused variable" warning
    (void)nextFRUAreaNewLoc;
#endif // ENABLE_FRU_AREA_RESIZE
    if (fruData.empty())
    {
        return false;
    }

    if (!ipmbWriteFru(static_cast<uint8_t>(bus), fruData))
    {
        return false;
    }

    // Rescan the bus so that GetRawFru dbus-call fetches updated values
    ipmbScanDevices(dbusInterfaceMap);
    return true;
}

int main()
{
    systemBus->request_name("xyz.openbmc_project.Ipmb.FruDevice");

    // this is a map with keys of pair(bus number, address) and values of
    // the object on dbus
    boost::container::flat_map<std::pair<size_t, size_t>,
                               std::shared_ptr<sdbusplus::asio::dbus_interface>>
        dbusInterfaceMap;

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface("/xyz/openbmc_project/Ipmb/FruDevice",
                                "xyz.openbmc_project.Ipmb.FruDeviceManager");

    iface->register_method("ReScan",
                           [&]() { ipmbScanDevices(dbusInterfaceMap); });

    iface->register_method("GetRawFru", getFRUInfo);

    iface->register_method(
        "WriteFru", [&](const uint8_t bus, const std::vector<uint8_t>& data) {
            if (!ipmbWriteFru(bus, data))
            {
                throw std::invalid_argument("Invalid Arguments.");
            }
            // schedule rescan on success
            ipmbScanDevices(dbusInterfaceMap);
        });

    iface->initialize();

    // run the initial scan
    ipmbScanDevices(dbusInterfaceMap);
    io.run();
    return 0;
}
