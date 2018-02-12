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

#include <Utils.hpp>
#include <boost/container/flat_map.hpp>
#include <ctime>
#include <dbus/connection.hpp>
#include <dbus/endpoint.hpp>
#include <dbus/message.hpp>
#include <dbus/properties.hpp>
#include <fcntl.h>
#include <fstream>
#include <future>
#include <linux/i2c-dev-user.h>
#include <iostream>
#include <sys/ioctl.h>
#include <regex>

namespace fs = std::experimental::filesystem;
static constexpr bool DEBUG = false;
static size_t UNKNOWN_BUS_OBJECT_COUNT = 0;

const static constexpr char *BASEBOARD_FRU_LOCATION =
    "/etc/fru/baseboard.fru.bin";

static constexpr std::array<const char *, 5> FRU_AREAS = {
    "INTERNAL", "CHASSIS", "BOARD", "PRODUCT", "MULTIRECORD"};

using DeviceMap = boost::container::flat_map<int, std::vector<char>>;
using BusMap = boost::container::flat_map<int, std::shared_ptr<DeviceMap>>;

int get_bus_frus(int file, int first, int last, int bus,
                 std::shared_ptr<DeviceMap> devices)
{
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> block_data;

    for (int ii = first; ii <= last; ii++)
    {
        // Set slave address
        if (ioctl(file, I2C_SLAVE_FORCE, ii) < 0)
        {
            std::cerr << "device at bus " << bus << "register " << ii
                      << "busy\n";
            continue;
        }
        // probe
        else if (i2c_smbus_read_byte(file) < 0)
        {
            continue;
        }

        if (DEBUG)
        {
            std::cout << "something at bus " << bus << "addr " << ii << "\n";
        }
        if (i2c_smbus_read_i2c_block_data(file, 0x0, 0x8, block_data.data()) <
            0)
        {
            std::cerr << "failed to read bus " << bus << " address " << ii
                      << "\n";
            continue;
        }
        size_t sum = 0;
        for (int jj = 0; jj < 7; jj++)
        {
            sum += block_data[jj];
        }
        sum = (256 - sum) & 0xFF;

        // check the header checksum
        if (sum == block_data[7])
        {
            std::vector<char> device;
            device.insert(device.end(), block_data.begin(),
                          block_data.begin() + 8);

            for (int jj = 1; jj <= FRU_AREAS.size(); jj++)
            {
                auto area_offset = device[jj];
                if (area_offset != 0)
                {
                    area_offset *= 8;
                    if (i2c_smbus_read_i2c_block_data(file, area_offset, 0x8,
                                                      block_data.data()) < 0)
                    {
                        std::cerr << "failed to read bus " << bus << " address "
                                  << ii << "\n";
                        return -1;
                    }
                    int length = block_data[1] * 8;
                    device.insert(device.end(), block_data.begin(),
                                  block_data.begin() + 8);
                    length -= 8;
                    area_offset += 8;

                    while (length > 0)
                    {
                        auto to_get = std::min(0x20, length);
                        if (i2c_smbus_read_i2c_block_data(
                                file, area_offset, to_get, block_data.data()) <
                            0)
                        {
                            std::cerr << "failed to read bus " << bus
                                      << " address " << ii << "\n";
                            return -1;
                        }
                        device.insert(device.end(), block_data.begin(),
                                      block_data.begin() + to_get);
                        area_offset += to_get;
                        length -= to_get;
                    }
                }
            }
            (*devices).emplace(ii, device);
        }
    }

    return 0;
}

static BusMap FindI2CDevices(const std::vector<fs::path> &i2cBuses)
{
    static std::vector<std::future<void>> futures;
    BusMap busMap;
    for (auto &i2cBus : i2cBuses)
    {
        auto busnum = i2cBus.string();
        auto lastDash = busnum.rfind(std::string("-"));
        // delete everything before dash inclusive
        if (lastDash != std::string::npos)
        {
            busnum.erase(0, lastDash + 1);
        }
        auto bus = std::stoi(busnum);

        auto file = open(i2cBus.c_str(), O_RDWR);
        if (file < 0)
        {
            std::cerr << "unable to open i2c device " << i2cBus.string()
                      << "\n";
            continue;
        }
        unsigned long funcs = 0;

        if (ioctl(file, I2C_FUNCS, &funcs) < 0)
        {
            std::cerr
                << "Error: Could not get the adapter functionality matrix bus"
                << bus << "\n";
            continue;
        }
        if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE) ||
            !(I2C_FUNC_SMBUS_READ_I2C_BLOCK))
        {
            std::cerr << "Error: Can't use SMBus Receive Byte command bus "
                      << bus << "\n";
            continue;
        }
        auto &device = busMap[bus];
        device = std::make_shared<DeviceMap>();

        // todo: call with boost asio?
        futures.emplace_back(
            std::async(std::launch::async, [file, device, bus] {
                //  i2cdetect by default uses the range 0x03 to 0x77, as this is
                //  what we
                //  have tested with, use this range. Could be changed in
                //  future.
                get_bus_frus(file, 0x03, 0x77, bus, device);
                close(file);
            }));
    }
    for (auto &fut : futures)
    {
        fut.get(); // wait for all scans
    }
    return busMap;
}

static const std::tm intelEpoch(void)
{
    std::tm val = {0};
    val.tm_year = 1996 - 1900;
    return val;
}

bool formatFru(const std::vector<char> &fruBytes,
               boost::container::flat_map<std::string, std::string> &result)
{
    static const std::vector<const char *> CHASSIS_FRU_AREAS = {
        "PART_NUMBER", "SERIAL_NUMBER", "CHASSIS_INFO_AM1", "CHASSIS_INFO_AM2"};

    static const std::vector<const char *> BOARD_FRU_AREAS = {
        "MANUFACTURER", "PRODUCT_NAME", "SERIAL_NUMBER", "PART_NUMBER",
        "VERSION_ID"};

    static const std::vector<const char *> PRODUCT_FRU_AREAS = {
        "MANUFACTURER",    "PRODUCT_NAME",          "PART_NUMBER",
        "PRODUCT_VERSION", "PRODUCT_SERIAL_NUMBER", "ASSET_TAG"};

    size_t sum = 0;

    if (fruBytes.size() < 8)
    {
        return false;
    }
    std::vector<char>::const_iterator fruAreaOffsetField = fruBytes.begin();
    result["Common Format Version"] =
        std::to_string(static_cast<int>(*fruAreaOffsetField));

    const std::vector<const char *> *fieldData;

    for (auto &area : FRU_AREAS)
    {
        fruAreaOffsetField++;
        if (fruAreaOffsetField >= fruBytes.end())
        {
            return false;
        }
        size_t offset = (*fruAreaOffsetField) * 8;

        if (offset > 1)
        {
            // +2 to skip format and length
            std::vector<char>::const_iterator fruBytesIter =
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
                time_t timeValue = mktime(&fruTime);
                timeValue += minutes * 60;
                fruTime = *gmtime(&timeValue);

                result["BOARD_MANUFACTURE_DATE"] = asctime(&fruTime);
                result["BOARD_MANUFACTURE_DATE"]
                    .pop_back(); // remove trailing null
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
            for (auto &field : *fieldData)
            {
                if (fruBytesIter >= fruBytes.end())
                {
                    return false;
                }

                size_t length = *fruBytesIter & 0x3f;
                fruBytesIter += 1;

                if (fruBytesIter >= fruBytes.end())
                {
                    return false;
                }

                result[std::string(area) + "_" + field] =
                    std::string(fruBytesIter, fruBytesIter + length);
                fruBytesIter += length;
                if (fruBytesIter >= fruBytes.end())
                {
                    std::cerr << "Warning Fru Length Mismatch:\n    ";
                    for (auto &c : fruBytes)
                    {
                        std::cerr << c;
                    }
                    std::cerr << "\n";
                    if (DEBUG)
                    {
                        for (auto &keyPair : result)
                        {
                            std::cerr << keyPair.first << " : "
                                      << keyPair.second << "\n";
                        }
                    }
                    return false;
                }
            }
        }
    }

    return true;
}

void AddFruObjectToDbus(
    std::shared_ptr<dbus::connection> dbusConn, std::vector<char> &device,
    dbus::DbusObjectServer &objServer,
    boost::container::flat_map<std::pair<size_t, size_t>,
                               std::shared_ptr<dbus::DbusObject>>
        &dbusObjectMap,
    int bus, size_t address)
{
    boost::container::flat_map<std::string, std::string> formattedFru;
    if (!formatFru(device, formattedFru))
    {
        std::cerr << "failed to format fru for device at bus " << std::hex
                  << bus << "address " << address << "\n";
        return;
    }
    auto productNameFind = formattedFru.find("BOARD_PRODUCT_NAME");
    std::string productName;
    if (productNameFind == formattedFru.end())
    {
        productNameFind = formattedFru.find("PRODUCT_PRODUCT_NAME");
    }
    if (productNameFind != formattedFru.end())
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

    auto object =
        objServer.add_object("/xyz/openbmc_project/FruDevice/" + productName);
    dbusObjectMap[std::pair<size_t, size_t>(bus, address)] = object;

    auto iface = std::make_shared<dbus::DbusInterface>(
        "xyz.openbmc_project.FruDevice", dbusConn);
    object->register_interface(iface);
    for (auto &property : formattedFru)
    {
        iface->set_property(property.first, property.second);
    }
    // baseboard can set this to -1 to not set a bus / address
    if (bus > 0)
    {
        std::stringstream data;
        data << "0x" << std::hex << bus;
        iface->set_property("BUS", data.str());
        data.str("");
        data << "0x" << std::hex << address;
        iface->set_property("ADDRESS", data.str());
    }
}

static bool readBaseboardFru(std::vector<char> &baseboardFru)
{
    // try to read baseboard fru from file
    std::ifstream baseboardFruFile(BASEBOARD_FRU_LOCATION, std::ios::binary);
    if (baseboardFruFile.good())
    {
        baseboardFruFile.seekg(0, std::ios_base::end);
        std::streampos fileSize = baseboardFruFile.tellg();
        baseboardFru.resize(fileSize);
        baseboardFruFile.seekg(0, std::ios_base::beg);
        baseboardFruFile.read(baseboardFru.data(), fileSize);
    }
    else
    {
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    auto devDir = fs::path("/dev/");
    auto matchString = std::string("i2c*");
    std::vector<fs::path> i2cBuses;

    if (!find_files(devDir, matchString, i2cBuses, 0))
    {
        std::cerr << "unable to find i2c devices\n";
        return 1;
    }
    BusMap busMap = FindI2CDevices(i2cBuses);

    boost::asio::io_service io;
    auto systemBus = std::make_shared<dbus::connection>(io, dbus::bus::system);
    dbus::DbusObjectServer objServer(systemBus);
    systemBus->request_name("com.intel.FruDevice");

    // this is a map with keys of pair(bus number, adddress) and values of the
    // object on dbus
    boost::container::flat_map<std::pair<size_t, size_t>,
                               std::shared_ptr<dbus::DbusObject>>
        dbusObjectMap;

    for (auto &devicemap : busMap)
    {
        for (auto &device : *devicemap.second)
        {
            AddFruObjectToDbus(systemBus, device.second, objServer,
                               dbusObjectMap, devicemap.first, device.first);
        }
    }

    std::vector<char> baseboardFru;
    if (readBaseboardFru(baseboardFru))
    {
        AddFruObjectToDbus(systemBus, baseboardFru, objServer, dbusObjectMap,
                           -1, -1);
    }

    auto object = std::make_shared<dbus::DbusObject>(
        systemBus, "/xyz/openbmc_project/FruDevice");
    objServer.register_object(object);
    auto iface = std::make_shared<dbus::DbusInterface>(
        "xyz.openbmc_project.FruDeviceManager", systemBus);
    object->register_interface(iface);

    iface->register_method("ReScan", [&]() {
        busMap = FindI2CDevices(i2cBuses);

        for (auto &busObj : dbusObjectMap)
        {
            objServer.remove_object(busObj.second);
        }
        dbusObjectMap.clear();
        UNKNOWN_BUS_OBJECT_COUNT = 0;

        for (auto &devicemap : busMap)
        {
            for (auto &device : *devicemap.second)
            {
                AddFruObjectToDbus(systemBus, device.second, objServer,
                                   dbusObjectMap, devicemap.first,
                                   device.first);
            }
        }

        return std::tuple<>(); // this is a bug in boost-dbus, needs some sort
                               // of return
    });

    io.run();
    return 0;
}
