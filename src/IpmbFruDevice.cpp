/*
// Copyright (c)
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

static constexpr std::array<const char*, 5> FRU_AREAS = {
    "INTERNAL", "CHASSIS", "BOARD", "PRODUCT", "MULTIRECORD"};
const static std::regex NON_ASCII_REGEX("[^\x01-\x7f]");
using DeviceMap = boost::container::flat_map<int, std::vector<uint8_t>>;
using BusMap = boost::container::flat_map<int, std::shared_ptr<DeviceMap>>;

static BusMap busMap;

boost::asio::io_service io;
auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
auto objServer = sdbusplus::asio::object_server(systemBus);

static const std::vector<const char*> CHASSIS_FRU_AREAS = {
    "PART_NUMBER", "SERIAL_NUMBER", "INFO_AM1", "INFO_AM2"};

static const std::vector<const char*> BOARD_FRU_AREAS = {
    "MANUFACTURER",   "PRODUCT_NAME", "SERIAL_NUMBER", "PART_NUMBER",
    "FRU_VERSION_ID", "INFO_AM1",     "INFO_AM2"};

static const std::vector<const char*> PRODUCT_FRU_AREAS = {
    "MANUFACTURER", "PRODUCT_NAME",   "PART_NUMBER", "VERSION", "SERIAL_NUMBER",
    "ASSET_TAG",    "FRU_VERSION_ID", "INFO_AM1",    "INFO_AM2"};

bool formatFru(const std::vector<uint8_t>& fruBytes,
               boost::container::flat_map<std::string, std::string>& result);

size_t calculateChecksum(std::vector<uint8_t>& fruAreaData);

std::vector<uint8_t>& getFruInfo(const uint8_t& bus, const uint8_t& address);

void AddFruObjectToDbus(
    std::vector<uint8_t>& device,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    uint32_t bus, uint32_t address);

bool updateFruProperty(
    const std::string& assetTag, uint32_t bus, uint32_t address,
    std::string propertyName,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap);

static bool isMuxBus(size_t bus)
{
    return is_symlink(std::filesystem::path(
        "/sys/bus/i2c/devices/i2c-" + std::to_string(bus) + "/mux_device"));
}

static const std::tm intelEpoch(void)
{
    std::tm val = {};
    val.tm_year = 1996 - 1900;
    return val;
}

static char sixBitToChar(uint8_t val)
{
    return static_cast<char>((val & 0x3f) + ' ');
}

/* 0xd - 0xf are reserved values, but not fatal; use a placeholder char. */
static const char bcdHighChars[] = {
    ' ', '-', '.', 'X', 'X', 'X',
};

static char bcdPlusToChar(uint8_t val)
{
    val &= 0xf;
    return (val < 10) ? static_cast<char>(val + '0') : bcdHighChars[val - 10];
}

enum class DecodeState
{
    ok,
    end,
    err,
};

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

static std::pair<DecodeState, std::string>
    decodeFruData(std::vector<uint8_t>::const_iterator& iter,
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
        std::cerr << "FRU data field extends past end of FRU data\n";
        return make_pair(DecodeState::err, value);
    }

    switch (type)
    {
        case FRUDataEncoding::binary:
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

bool formatFru(const std::vector<uint8_t>& fruBytes,
               boost::container::flat_map<std::string, std::string>& result)
{
    if (fruBytes.size() <= 8)
    {
        return false;
    }
    std::vector<uint8_t>::const_iterator fruAreaOffsetField = fruBytes.begin();
    result["Common_Format_Version"] =
        std::to_string(static_cast<int>(*fruAreaOffsetField));

    const std::vector<const char*>* fieldData;

    for (const std::string& area : FRU_AREAS)
    {
        ++fruAreaOffsetField;
        if (fruAreaOffsetField >= fruBytes.end())
        {
            return false;
        }
        size_t offset = (*fruAreaOffsetField) * 8;

        if (offset > 1)
        {
            // +2 to skip format and length
            std::vector<uint8_t>::const_iterator fruBytesIter =
                fruBytes.begin() + offset + 2;

            if (fruBytesIter >= fruBytes.end())
            {
                return false;
            }

            if (area == "CHASSIS")
            {
                result["CHASSIS_TYPE"] =
                    std::to_string(static_cast<int>(*fruBytesIter));
                fruBytesIter += 1;
                fieldData = &CHASSIS_FRU_AREAS;
            }
            else if (area == "BOARD")
            {
                result["BOARD_LANGUAGE_CODE"] =
                    std::to_string(static_cast<int>(*fruBytesIter));
                fruBytesIter += 1;
                if (fruBytesIter >= fruBytes.end())
                {
                    return false;
                }

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
                fieldData = &BOARD_FRU_AREAS;
            }
            else if (area == "PRODUCT")
            {
                result["PRODUCT_LANGUAGE_CODE"] =
                    std::to_string(static_cast<int>(*fruBytesIter));
                fruBytesIter += 1;
                fieldData = &PRODUCT_FRU_AREAS;
            }
            else
            {
                continue;
            }
            for (auto& field : *fieldData)
            {
                auto res = decodeFruData(fruBytesIter, fruBytes.end());
                std::string name = area + "_" + field;

                if (res.first == DecodeState::end)
                {
                    break;
                }
                else if (res.first == DecodeState::err)
                {
                    std::cerr << "Error while parsing " << name << "\n";
                    return false;
                }

                std::string value = res.second;

                // Strip non null characters from the end
                value.erase(std::find_if(value.rbegin(), value.rend(),
                                         [](char ch) { return ch != 0; })
                                .base(),
                            value.end());

                result[name] = std::move(value);
            }
        }
    }

    return true;
}

std::vector<uint8_t>& getFruInfo(const uint8_t& bus, const uint8_t& address)
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

void AddFruObjectToDbus(
    std::vector<uint8_t>& device,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    uint32_t bus, uint32_t address)
{

    boost::container::flat_map<std::string, std::string> formattedFru;
    if (!formatFru(device, formattedFru))
    {
        std::cerr << "failed to format fru for device at bus " << bus
                  << " address " << address << "\n";
        return;
    }

    auto productNameFind = formattedFru.find("BOARD_PRODUCT_NAME");
    std::string productName;
    // Not found under Board section or an empty string.
    if (productNameFind == formattedFru.end() ||
        productNameFind->second.empty())
    {
        productNameFind = formattedFru.find("PRODUCT_PRODUCT_NAME");
    }
    // Found under Product section and not an empty string.
    if (productNameFind != formattedFru.end() &&
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

    productName = "/xyz/openbmc_project/Ipmb/FruDevice/" + productName;
    // avoid duplicates by checking to see if on a mux
    if (bus > 0)
    {
        int highest = -1;
        bool found = false;

        for (auto const& busIface : dbusInterfaceMap)
        {

            std::string path = busIface.second->get_object_path();
            if (std::regex_match(path, std::regex(productName + "(_\\d+|)$")))
            {

                if (isMuxBus(bus) && address == busIface.first.second &&
                    (getFruInfo(static_cast<uint8_t>(busIface.first.first),
                                static_cast<uint8_t>(busIface.first.second)) ==
                     getFruInfo(static_cast<uint8_t>(bus),
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
            // We found something with the same name.  If highest was still -1,
            // it means this new entry will be _0.
            productName += "_";
            productName += std::to_string(++highest);
        }
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface(productName,
                                "xyz.openbmc_project.Ipmb.FruDevice");
    dbusInterfaceMap[std::pair<size_t, size_t>(bus, address)] = iface;

    for (auto& property : formattedFru)
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
                        if (updateFruProperty(req, bus, address, propertyName,
                                              dbusInterfaceMap))
                        {
                            resp = req;
                        }
                        else
                        {
                            throw std::invalid_argument(
                                "Fru property update failed.");
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

void ipmbWrite(uint8_t ipmbAddress, uint8_t offset,
               const std::vector<uint8_t>& fru)
{
    uint8_t cmd = 0x12;
    uint8_t netFn = 0xa;
    uint8_t lun = 0;
    uint8_t iter = 0;
    std::vector<uint8_t> cmdData;
    std::vector<uint8_t> respData;

    cmdData.emplace_back(fru.size() + 3);
    cmdData.emplace_back(0);
    cmdData.emplace_back(offset);
    cmdData.emplace_back(0);
    cmdData.emplace_back(fru.size());

    for (iter = 0; iter < fru.size(); iter++)
    {
        cmdData.emplace_back(fru.at(iter));
    }

    auto method =
        systemBus->new_method_call("xyz.openbmc_project.Ipmi.Channel.Ipmb",
                                   "/xyz/openbmc_project/Ipmi/Channel/Ipmb",
                                   "org.openbmc.Ipmb", "sendRequest");

    method.append(ipmbAddress, netFn, lun, cmd, cmdData);

    auto reply = systemBus->call(method);
    if (reply.is_method_error())
    {
        std::cerr << "Error reading from Ipmb";
        return;
    }

    IpmbMethodType resp;
    reply.read(resp);

    respData =
        std::move(std::get<std::remove_reference_t<decltype(respData)>>(resp));

    return;
}

bool ipmbWriteFru(uint8_t bus, uint8_t address, const std::vector<uint8_t>& fru)
{
    uint8_t offset = 0;

    std::cout << "Write Addres : " << address << std::endl;
    boost::container::flat_map<std::string, std::string> tmp;
    if (fru.size() > MAX_FRU_SIZE)
    {
        std::cerr << "Invalid fru.size() during writeFru\n";
        return false;
    }
    // verify legal fru by running it through fru parsing logic
    if (!formatFru(fru, tmp))
    {
        std::cerr << "Invalid fru format during writeFru\n";
        return false;
    }

    ipmbWrite(ipmbAddress.at(bus), offset, fru);

    return true;
}

// Calculate new checksum for fru info area
size_t calculateChecksum(std::vector<uint8_t>& fruAreaData)
{
    constexpr int checksumMod = 256;
    constexpr uint8_t modVal = 0xFF;
    int sum = std::accumulate(fruAreaData.begin(), fruAreaData.end(), 0);
    return (checksumMod - sum) & modVal;
}

// Update new fru area length &
// Update checksum at new checksum location
static void updateFruAreaLenAndChecksum(std::vector<uint8_t>& fruData,
                                        size_t fruAreaStartOffset,
                                        size_t fruAreaNoMoreFieldOffset)
{
    constexpr size_t fruAreaMultipleBytes = 8;
    size_t traverseFruAreaIndex = fruAreaNoMoreFieldOffset - fruAreaStartOffset;

    // fill zeros for any remaining unused space
    std::fill(fruData.begin() + fruAreaNoMoreFieldOffset, fruData.end(), 0);

    size_t mod = traverseFruAreaIndex % fruAreaMultipleBytes;
    size_t checksumLoc;
    if (!mod)
    {
        traverseFruAreaIndex += (fruAreaMultipleBytes);
        checksumLoc = fruAreaNoMoreFieldOffset + (fruAreaMultipleBytes - 1);
    }
    else
    {
        traverseFruAreaIndex += (fruAreaMultipleBytes - mod);
        checksumLoc =
            fruAreaNoMoreFieldOffset + (fruAreaMultipleBytes - mod - 1);
    }

    size_t newFruAreaLen = (traverseFruAreaIndex / fruAreaMultipleBytes) +
                           ((traverseFruAreaIndex % fruAreaMultipleBytes) != 0);
    size_t fruAreaLengthLoc = fruAreaStartOffset + 1;
    fruData[fruAreaLengthLoc] = static_cast<uint8_t>(newFruAreaLen);

    // Calculate new checksum
    std::vector<uint8_t> finalFruData;
    std::copy_n(fruData.begin() + fruAreaStartOffset,
                checksumLoc - fruAreaStartOffset,
                std::back_inserter(finalFruData));

    size_t checksumVal = calculateChecksum(finalFruData);

    fruData[checksumLoc] = static_cast<uint8_t>(checksumVal);
}

static size_t getTypeLength(uint8_t fruFieldTypeLenValue)
{
    constexpr uint8_t typeLenMask = 0x3F;
    return fruFieldTypeLenValue & typeLenMask;
}

void findIpmbDev(std::vector<uint8_t>& ipmbAddr)
{
    constexpr const char* configFilePath =
        "/usr/share/ipmbbridge/ipmb-channels.json";

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
        for (const auto& channelConfig : data["channels"])
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
    uint8_t cmd = 0x10;
    uint8_t netFn = 0xa;
    static const uint8_t lun = 0;

    std::vector<uint8_t> cmdData{0};
    std::vector<uint8_t> respData;

    auto method =
        systemBus->new_method_call("xyz.openbmc_project.Ipmi.Channel.Ipmb",
                                   "/xyz/openbmc_project/Ipmi/Channel/Ipmb",
                                   "org.openbmc.Ipmb", "sendRequest");

    method.append(ipmbAddress, netFn, lun, cmd, cmdData);

    auto reply = systemBus->call(method);
    if (reply.is_method_error())
    {
        std::cerr << "Error reading from Ipmb";
        return;
    }

    IpmbMethodType resp;
    reply.read(resp);

    // Assign Response completion code
    compCode = std::get<4>(resp);

    std::vector<uint8_t> data;
    data = std::get<5>(resp);

    readLen = ((data.at(1) << 8) | data.at(0));

    respData =
        std::move(std::get<std::remove_reference_t<decltype(respData)>>(resp));
}

void ipmbReadFru(uint8_t ipmbAddress, uint8_t offset,
                 std::vector<uint8_t>& fruResponse)
{
    uint8_t cmd = 0x11;
    uint8_t netFn = 0xa;
    uint8_t lun = 0;
    uint8_t iter = 0;
    std::vector<uint8_t> cmdData;
    std::vector<uint8_t> respData;

    cmdData.emplace_back(0);
    cmdData.emplace_back(offset);
    cmdData.emplace_back(0);
    cmdData.emplace_back(49);

    auto method =
        systemBus->new_method_call("xyz.openbmc_project.Ipmi.Channel.Ipmb",
                                   "/xyz/openbmc_project/Ipmi/Channel/Ipmb",
                                   "org.openbmc.Ipmb", "sendRequest");

    method.append(ipmbAddress, netFn, lun, cmd, cmdData);

    auto reply = systemBus->call(method);
    if (reply.is_method_error())
    {
        std::cerr << "Error reading from Ipmb";
        return;
    }

    IpmbMethodType resp;
    reply.read(resp);

    respData =
        std::move(std::get<std::remove_reference_t<decltype(respData)>>(resp));

    respData.erase(respData.begin());

    for (iter = 0; iter < respData.size(); iter++)
    {
        fruResponse.push_back(respData.at(iter));
    }

    return;
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
        getAreaInfo(ipmbAddress.at(iter), compCode, readLen);

        if (compCode == 0)
        {
            while (offset < readLen)
            {
                ipmbReadFru(ipmbAddress.at(iter), offset, fruResponse);
                offset = offset + 49;
            }
        }

        hostDev[0] = fruResponse;
        busMap[count++] = std::make_shared<DeviceMap>(hostDev);

        offset = 0;
        compCode = 0;
        fruResponse.clear();
    }

    for (auto& devicemap : busMap)
    {
        for (auto& device : *devicemap.second)
        {
            AddFruObjectToDbus(device.second, dbusInterfaceMap, devicemap.first,
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

bool updateFruProperty(
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
            << "Fru field data cannot be of 1 char or more than 63 chars. "
               "Invalid Length "
            << updatePropertyReqLen << "\n";
        return false;
    }

    std::vector<uint8_t> fruData;
    try
    {
        fruData = getFruInfo(static_cast<uint8_t>(bus),
                             static_cast<uint8_t>(address));
    }
    catch (std::invalid_argument& e)
    {
        std::cerr << "Failure getting Fru Info" << e.what() << "\n";
        return false;
    }

    if (fruData.empty())
    {
        return false;
    }

    const std::vector<const char*>* fieldData;

    constexpr size_t commonHeaderMultipleBytes = 8; // in multiple of 8 bytes
    uint8_t fruAreaOffsetFieldValue = 1;
    size_t offset = 0;

    if (propertyName.find("CHASSIS") == 0)
    {
        constexpr size_t fruAreaOffsetField = 2;
        fruAreaOffsetFieldValue = fruData[fruAreaOffsetField];

        offset = 3; // chassis part number offset. Skip fixed first 3 bytes

        fieldData = &CHASSIS_FRU_AREAS;
    }
    else if (propertyName.find("BOARD") == 0)
    {
        constexpr size_t fruAreaOffsetField = 3;
        fruAreaOffsetFieldValue = fruData[fruAreaOffsetField];

        offset = 6; // board manufacturer offset. Skip fixed first 6 bytes

        fieldData = &BOARD_FRU_AREAS;
    }
    else if (propertyName.find("PRODUCT") == 0)
    {
        constexpr size_t fruAreaOffsetField = 4;
        fruAreaOffsetFieldValue = fruData[fruAreaOffsetField];

        offset = 3;
        // Manufacturer name offset. Skip fixed first 3 product fru bytes i.e.
        // version, area length and language code

        fieldData = &PRODUCT_FRU_AREAS;
    }
    else
    {
        std::cerr << "ProperyName doesn't exist in Fru Area Vector \n";
        return false;
    }

    size_t fruAreaStartOffset =
        fruAreaOffsetFieldValue * commonHeaderMultipleBytes;
    size_t fruDataIter = fruAreaStartOffset + offset;
    size_t fruUpdateFieldLoc = 0;
    size_t typeLength;
    size_t skipToFruUpdateField = 0;

    for (auto& field : *fieldData)
    {
        skipToFruUpdateField++;
        if (propertyName.find(field) != std::string::npos)
        {
            break;
        }
    }

    for (size_t i = 1; i < skipToFruUpdateField; i++)
    {
        typeLength = getTypeLength(fruData[fruDataIter]);
        fruUpdateFieldLoc = fruDataIter + typeLength;
        fruDataIter = ++fruUpdateFieldLoc;
    }

    // Push post update fru field bytes to a vector
    typeLength = getTypeLength(fruData[fruUpdateFieldLoc]);
    size_t postFruUpdateFieldLoc = fruUpdateFieldLoc + typeLength;
    postFruUpdateFieldLoc++;
    skipToFruUpdateField++;
    fruDataIter = postFruUpdateFieldLoc;
    size_t endLengthLoc = 0;
    size_t fruFieldLoc = 0;
    constexpr uint8_t endLength = 0xC1;
    for (size_t i = fruDataIter; i < fruData.size(); i++)
    {
        typeLength = getTypeLength(fruData[fruDataIter]);
        fruFieldLoc = fruDataIter + typeLength;
        fruDataIter = ++fruFieldLoc;
        if (fruData[fruDataIter] == endLength)
        {
            endLengthLoc = fruDataIter;
            break;
        }
    }
    endLengthLoc++;

    std::vector<uint8_t> postUpdatedFruData;
    std::copy_n(fruData.begin() + postFruUpdateFieldLoc,
                endLengthLoc - postFruUpdateFieldLoc,
                std::back_inserter(postUpdatedFruData));

    // write new requested property field length and data
    constexpr uint8_t newTypeLenMask = 0xC0;
    fruData[fruUpdateFieldLoc] =
        static_cast<uint8_t>(updatePropertyReqLen | newTypeLenMask);
    fruUpdateFieldLoc++;
    std::copy(updatePropertyReq.begin(), updatePropertyReq.end(),
              fruData.begin() + fruUpdateFieldLoc);

    // Copy remaining data to main fru area - post updated fru field vector
    postFruUpdateFieldLoc = fruUpdateFieldLoc + updatePropertyReqLen;
    size_t fruAreaEndOffset = postFruUpdateFieldLoc + postUpdatedFruData.size();
    if (fruAreaEndOffset > fruData.size())
    {
        std::cerr << "Fru area offset: " << fruAreaEndOffset
                  << "should not be greater than Fru data size: "
                  << fruData.size() << std::endl;
        throw std::invalid_argument(
            "Fru area offset should not be greater than Fru data size");
    }
    std::copy(postUpdatedFruData.begin(), postUpdatedFruData.end(),
              fruData.begin() + postFruUpdateFieldLoc);

    // Update final fru with new fru area length and checksum
    updateFruAreaLenAndChecksum(fruData, fruAreaStartOffset, fruAreaEndOffset);

    if (fruData.empty())
    {
        return false;
    }

    if (!ipmbWriteFru(static_cast<uint8_t>(bus), static_cast<uint8_t>(address),
                      fruData))
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

    iface->register_method("GetRawFru", getFruInfo);

    iface->register_method("WriteFru", [&](const uint8_t bus,
                                           const uint8_t address,
                                           const std::vector<uint8_t>& data) {
        if (!ipmbWriteFru(bus, address, data))
        {
            throw std::invalid_argument("Invalid Arguments.");
            return;
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
