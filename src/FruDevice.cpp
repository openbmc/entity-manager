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
/// \file FruDevice.cpp

#include "Common.hpp"
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

namespace fs = std::filesystem;
static constexpr bool DEBUG = false;
static size_t UNKNOWN_BUS_OBJECT_COUNT = 0;
constexpr size_t MAX_FRU_SIZE = 512;
constexpr size_t MAX_EEPROM_PAGE_INDEX = 255;
constexpr size_t busTimeoutSeconds = 5;

constexpr const char* blacklistPath = PACKAGE_DIR "blacklist.json";

const static constexpr char* BASEBOARD_FRU_LOCATION =
    "/etc/fru/baseboard.fru.bin";

const static constexpr char* I2C_DEV_LOCATION = "/dev";

using DeviceMap = boost::container::flat_map<int, std::vector<uint8_t>>;
using BusMap = boost::container::flat_map<int, std::shared_ptr<DeviceMap>>;

static std::set<size_t> busBlacklist;
struct FindDevicesWithCallback;

static BusMap busMap;

static bool powerIsOn = false;

static boost::container::flat_map<
    std::pair<size_t, size_t>, std::shared_ptr<sdbusplus::asio::dbus_interface>>
    foundDevices;

static boost::container::flat_map<size_t, std::set<size_t>> failedAddresses;

boost::asio::io_service io;
auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
auto objServer = sdbusplus::asio::object_server(systemBus);

bool validateHeader(const std::array<uint8_t, I2C_SMBUS_BLOCK_MAX>& blockData);
uint8_t calculateChecksum(std::vector<uint8_t>::const_iterator iter,
                          std::vector<uint8_t>::const_iterator end);
bool updateFRUProperty(
    const std::string& assetTag, uint32_t bus, uint32_t address,
    std::string propertyName,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap);

using ReadBlockFunc =
    std::function<int64_t(int flag, int file, uint16_t address, uint16_t offset,
                          uint8_t length, uint8_t* outBuf)>;

// Read and validate FRU contents, given the flag required for raw i2c, the open
// file handle, a read method, and a helper string for failures.
std::vector<uint8_t> readFRUContents(int flag, int file, uint16_t address,
                                     ReadBlockFunc readBlock,
                                     const std::string& errorHelp)
{
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;

    if (readBlock(flag, file, address, 0x0, 0x8, blockData.data()) < 0)
    {
        std::cerr << "failed to read " << errorHelp << "\n";
        return {};
    }

    // check the header checksum
    if (!validateHeader(blockData))
    {
        if (DEBUG)
        {
            std::cerr << "Illegal header " << errorHelp << "\n";
        }

        return {};
    }

    std::vector<uint8_t> device;
    device.insert(device.end(), blockData.begin(), blockData.begin() + 8);

    bool hasMultiRecords = false;
    size_t fruLength = fruBlockSize; // At least FRU header is present
    for (fruAreas area = fruAreas::fruAreaInternal;
         area <= fruAreas::fruAreaMultirecord; ++area)
    {
        // Offset value can be 255.
        unsigned int areaOffset = device[getHeaderAreaFieldOffset(area)];
        if (areaOffset == 0)
        {
            continue;
        }

        // MultiRecords are different. area is not tracking section, it's
        // walking the common header.
        if (area == fruAreas::fruAreaMultirecord)
        {
            hasMultiRecords = true;
            break;
        }

        areaOffset *= fruBlockSize;

        if (readBlock(flag, file, address, static_cast<uint16_t>(areaOffset),
                      0x2, blockData.data()) < 0)
        {
            std::cerr << "failed to read " << errorHelp << "\n";
            return {};
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
            if (readBlock(flag, file, address,
                          static_cast<uint16_t>(areaOffset), 0x3,
                          blockData.data()) < 0)
            {
                std::cerr << "failed to read " << errorHelp << "\n";
                return {};
            }

            // Ok, let's check the record length, which is in bytes (unsigned,
            // up to 255, so blockData should hold uint8_t not char)
            size_t recordLength = blockData[2];
            areaOffset += (recordLength + multiRecordHeaderSize);
            fruLength = (areaOffset > fruLength) ? areaOffset : fruLength;

            // If this is the end of the list bail.
            if ((blockData[1] & multiRecordEndOfListMask))
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

        if (readBlock(flag, file, address, static_cast<uint16_t>(readOffset),
                      static_cast<uint8_t>(requestLength),
                      blockData.data()) < 0)
        {
            std::cerr << "failed to read " << errorHelp << "\n";
            return {};
        }

        device.insert(device.end(), blockData.begin(),
                      blockData.begin() + requestLength);

        readOffset += requestLength;
        fruLength -= std::min(requestLength, fruLength);
    }

    return device;
}

// Given a bus/address, produce the path in sysfs for an eeprom.
static std::string getEepromPath(size_t bus, size_t address)
{
    std::stringstream output;
    output << "/sys/bus/i2c/devices/" << bus << "-" << std::right
           << std::setfill('0') << std::setw(4) << std::hex << address
           << "/eeprom";
    return output.str();
}

static bool hasEepromFile(size_t bus, size_t address)
{
    auto path = getEepromPath(bus, address);
    try
    {
        return fs::exists(path);
    }
    catch (...)
    {
        return false;
    }
}

static int64_t readFromEeprom(int flag __attribute__((unused)), int fd,
                              uint16_t address __attribute__((unused)),
                              uint16_t offset, uint8_t len, uint8_t* buf)
{
    auto result = lseek(fd, offset, SEEK_SET);
    if (result < 0)
    {
        std::cerr << "failed to seek\n";
        return -1;
    }

    return read(fd, buf, len);
}

static int busStrToInt(const std::string& busName)
{
    auto findBus = busName.rfind("-");
    if (findBus == std::string::npos)
    {
        return -1;
    }
    return std::stoi(busName.substr(findBus + 1));
}

static int getRootBus(size_t bus)
{
    auto ec = std::error_code();
    auto path = std::filesystem::read_symlink(
        std::filesystem::path("/sys/bus/i2c/devices/i2c-" +
                              std::to_string(bus) + "/mux_device"),
        ec);
    if (ec)
    {
        return -1;
    }

    std::string filename = path.filename();
    auto findBus = filename.find("-");
    if (findBus == std::string::npos)
    {
        return -1;
    }
    return std::stoi(filename.substr(0, findBus));
}

static void makeProbeInterface(size_t bus, size_t address)
{
    if (isMuxBus(bus))
    {
        return; // the mux buses are random, no need to publish
    }
    auto [it, success] = foundDevices.emplace(
        std::make_pair(bus, address),
        objServer.add_interface(
            "/xyz/openbmc_project/FruDevice/" + std::to_string(bus) + "_" +
                std::to_string(address),
            "xyz.openbmc_project.Inventory.Item.I2CDevice"));
    if (!success)
    {
        return; // already added
    }
    it->second->register_property("Bus", bus);
    it->second->register_property("Address", address);
    it->second->initialize();
}

static int isDevice16Bit(int file)
{
    /* Get first byte */
    int byte1 = i2c_smbus_read_byte_data(file, 0);
    if (byte1 < 0)
    {
        return byte1;
    }
    /* Read 7 more bytes, it will read same first byte in case of
     * 8 bit but it will read next byte in case of 16 bit
     */
    for (int i = 0; i < 7; i++)
    {
        int byte2 = i2c_smbus_read_byte_data(file, 0);
        if (byte2 < 0)
        {
            return byte2;
        }
        if (byte2 != byte1)
        {
            return 1;
        }
    }
    return 0;
}

// Issue an I2C transaction to first write to_slave_buf_len bytes,then read
// from_slave_buf_len bytes.
static int i2c_smbus_write_then_read(int file, uint16_t address,
                                     uint8_t* toSlaveBuf, uint8_t toSlaveBufLen,
                                     uint8_t* fromSlaveBuf,
                                     uint8_t fromSlaveBufLen)
{
    if (toSlaveBuf == NULL || toSlaveBufLen == 0 || fromSlaveBuf == NULL ||
        fromSlaveBufLen == 0)
    {
        return -1;
    }

#define SMBUS_IOCTL_WRITE_THEN_READ_MSG_COUNT 2
    struct i2c_msg msgs[SMBUS_IOCTL_WRITE_THEN_READ_MSG_COUNT];
    struct i2c_rdwr_ioctl_data rdwr;

    msgs[0].addr = address;
    msgs[0].flags = 0;
    msgs[0].len = toSlaveBufLen;
    msgs[0].buf = toSlaveBuf;
    msgs[1].addr = address;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = fromSlaveBufLen;
    msgs[1].buf = fromSlaveBuf;

    rdwr.msgs = msgs;
    rdwr.nmsgs = SMBUS_IOCTL_WRITE_THEN_READ_MSG_COUNT;

    int ret = ioctl(file, I2C_RDWR, &rdwr);

    return (ret == SMBUS_IOCTL_WRITE_THEN_READ_MSG_COUNT) ? ret : -1;
}

static int64_t readBlockData(int flag, int file, uint16_t address,
                             uint16_t offset, uint8_t len, uint8_t* buf)
{
    if (flag == 0)
    {
        return i2c_smbus_read_i2c_block_data(file, static_cast<uint8_t>(offset),
                                             len, buf);
    }

    offset = htobe16(offset);
    return i2c_smbus_write_then_read(
        file, address, reinterpret_cast<uint8_t*>(&offset), 2, buf, len);
}

bool validateHeader(const std::array<uint8_t, I2C_SMBUS_BLOCK_MAX>& blockData)
{
    // ipmi spec format version number is currently at 1, verify it
    if (blockData[0] != 0x1)
    {
        return false;
    }

    // verify pad is set to 0
    if (blockData[6] != 0x0)
    {
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
        return false;
    }
    return true;
}

// TODO: This code is very similar to the non-eeprom version and can be merged
// with some tweaks.
static std::vector<uint8_t> processEeprom(int bus, int address)
{
    auto path = getEepromPath(bus, address);

    int file = open(path.c_str(), O_RDONLY);
    if (file < 0)
    {
        std::cerr << "Unable to open eeprom file: " << path << "\n";
        return {};
    }

    std::string errorMessage = "eeprom at " + std::to_string(bus) +
                               " address " + std::to_string(address);
    std::vector<uint8_t> device = readFRUContents(
        0, file, static_cast<uint16_t>(address), readFromEeprom, errorMessage);

    close(file);
    return device;
}

std::set<int> findI2CEeproms(int i2cBus, std::shared_ptr<DeviceMap> devices)
{
    std::set<int> foundList;

    std::string path = "/sys/bus/i2c/devices/i2c-" + std::to_string(i2cBus);

    // For each file listed under the i2c device
    // NOTE: This should be faster than just checking for each possible address
    // path.
    for (const auto& p : fs::directory_iterator(path))
    {
        const std::string node = p.path().string();
        std::smatch m;
        bool found =
            std::regex_match(node, m, std::regex(".+\\d+-([0-9abcdef]+$)"));

        if (!found)
        {
            continue;
        }
        if (m.size() != 2)
        {
            std::cerr << "regex didn't capture\n";
            continue;
        }

        std::ssub_match subMatch = m[1];
        std::string addressString = subMatch.str();

        std::size_t ignored;
        const int hexBase = 16;
        int address = std::stoi(addressString, &ignored, hexBase);

        const std::string eeprom = node + "/eeprom";

        try
        {
            if (!fs::exists(eeprom))
            {
                continue;
            }
        }
        catch (...)
        {
            continue;
        }

        // There is an eeprom file at this address, it may have invalid
        // contents, but we found it.
        foundList.insert(address);

        std::vector<uint8_t> device = processEeprom(i2cBus, address);
        if (!device.empty())
        {
            devices->emplace(address, device);
        }
    }

    return foundList;
}

int getBusFRUs(int file, int first, int last, int bus,
               std::shared_ptr<DeviceMap> devices)
{

    std::future<int> future = std::async(std::launch::async, [&]() {
        // NOTE: When reading the devices raw on the bus, it can interfere with
        // the driver's ability to operate, therefore read eeproms first before
        // scanning for devices without drivers. Several experiments were run
        // and it was determined that if there were any devices on the bus
        // before the eeprom was hit and read, the eeprom driver wouldn't open
        // while the bus device was open. An experiment was not performed to see
        // if this issue was resolved if the i2c bus device was closed, but
        // hexdumps of the eeprom later were successful.

        // Scan for i2c eeproms loaded on this bus.
        std::set<int> skipList = findI2CEeproms(bus, devices);
        std::set<size_t>& failedItems = failedAddresses[bus];

        std::set<size_t>* rootFailures = nullptr;
        int rootBus = getRootBus(bus);

        if (rootBus >= 0)
        {
            rootFailures = &(failedAddresses[rootBus]);
        }

        constexpr int startSkipSlaveAddr = 0;
        constexpr int endSkipSlaveAddr = 12;

        for (int ii = first; ii <= last; ii++)
        {
            if (skipList.find(ii) != skipList.end())
            {
                continue;
            }
            // skipping since no device is present in this range
            if (ii >= startSkipSlaveAddr && ii <= endSkipSlaveAddr)
            {
                continue;
            }
            // Set slave address
            if (ioctl(file, I2C_SLAVE, ii) < 0)
            {
                std::cerr << "device at bus " << bus << " address " << ii
                          << " busy\n";
                continue;
            }
            // probe
            else if (i2c_smbus_read_byte(file) < 0)
            {
                continue;
            }

            if (DEBUG)
            {
                std::cout << "something at bus " << bus << " addr " << ii
                          << "\n";
            }

            makeProbeInterface(bus, ii);

            if (failedItems.find(ii) != failedItems.end())
            {
                // if we failed to read it once, unlikely we can read it later
                continue;
            }

            if (rootFailures != nullptr)
            {
                if (rootFailures->find(ii) != rootFailures->end())
                {
                    continue;
                }
            }

            /* Check for Device type if it is 8 bit or 16 bit */
            int flag = isDevice16Bit(file);
            if (flag < 0)
            {
                std::cerr << "failed to read bus " << bus << " address " << ii
                          << "\n";
                if (powerIsOn)
                {
                    failedItems.insert(ii);
                }
                continue;
            }

            std::string errorMessage =
                "bus " + std::to_string(bus) + " address " + std::to_string(ii);
            std::vector<uint8_t> device =
                readFRUContents(flag, file, static_cast<uint16_t>(ii),
                                readBlockData, errorMessage);
            if (device.empty())
            {
                continue;
            }

            devices->emplace(ii, device);
        }
        return 1;
    });
    std::future_status status =
        future.wait_for(std::chrono::seconds(busTimeoutSeconds));
    if (status == std::future_status::timeout)
    {
        std::cerr << "Error reading bus " << bus << "\n";
        if (powerIsOn)
        {
            busBlacklist.insert(bus);
        }
        close(file);
        return -1;
    }

    close(file);
    return future.get();
}

void loadBlacklist(const char* path)
{
    std::ifstream blacklistStream(path);
    if (!blacklistStream.good())
    {
        // File is optional.
        std::cerr << "Cannot open blacklist file.\n\n";
        return;
    }

    nlohmann::json data =
        nlohmann::json::parse(blacklistStream, nullptr, false);
    if (data.is_discarded())
    {
        std::cerr << "Illegal blacklist file detected, cannot validate JSON, "
                     "exiting\n";
        std::exit(EXIT_FAILURE);
    }

    // It's expected to have at least one field, "buses" that is an array of the
    // buses by integer. Allow for future options to exclude further aspects,
    // such as specific addresses or ranges.
    if (data.type() != nlohmann::json::value_t::object)
    {
        std::cerr << "Illegal blacklist, expected to read dictionary\n";
        std::exit(EXIT_FAILURE);
    }

    // If buses field is missing, that's fine.
    if (data.count("buses") == 1)
    {
        // Parse the buses array after a little validation.
        auto buses = data.at("buses");
        if (buses.type() != nlohmann::json::value_t::array)
        {
            // Buses field present but invalid, therefore this is an error.
            std::cerr << "Invalid contents for blacklist buses field\n";
            std::exit(EXIT_FAILURE);
        }

        // Catch exception here for type mis-match.
        try
        {
            for (const auto& bus : buses)
            {
                busBlacklist.insert(bus.get<size_t>());
            }
        }
        catch (const nlohmann::detail::type_error& e)
        {
            // Type mis-match is a critical error.
            std::cerr << "Invalid bus type: " << e.what() << "\n";
            std::exit(EXIT_FAILURE);
        }
    }

    return;
}

static void FindI2CDevices(const std::vector<fs::path>& i2cBuses,
                           BusMap& busmap)
{
    for (auto& i2cBus : i2cBuses)
    {
        int bus = busStrToInt(i2cBus);

        if (bus < 0)
        {
            std::cerr << "Cannot translate " << i2cBus << " to int\n";
            continue;
        }
        if (busBlacklist.find(bus) != busBlacklist.end())
        {
            continue; // skip previously failed busses
        }

        int rootBus = getRootBus(bus);
        if (busBlacklist.find(rootBus) != busBlacklist.end())
        {
            continue;
        }

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
                << "Error: Could not get the adapter functionality matrix bus "
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
        auto& device = busmap[bus];
        device = std::make_shared<DeviceMap>();

        //  i2cdetect by default uses the range 0x03 to 0x77, as
        //  this is  what we have tested with, use this range. Could be
        //  changed in future.
        if (DEBUG)
        {
            std::cerr << "Scanning bus " << bus << "\n";
        }

        // fd is closed in this function in case the bus locks up
        getBusFRUs(file, 0x03, 0x77, bus, device);

        if (DEBUG)
        {
            std::cerr << "Done scanning bus " << bus << "\n";
        }
    }
}

// this class allows an async response after all i2c devices are discovered
struct FindDevicesWithCallback :
    std::enable_shared_from_this<FindDevicesWithCallback>
{
    FindDevicesWithCallback(const std::vector<fs::path>& i2cBuses,
                            BusMap& busmap,
                            std::function<void(void)>&& callback) :
        _i2cBuses(i2cBuses),
        _busMap(busmap), _callback(std::move(callback))
    {}
    ~FindDevicesWithCallback()
    {
        _callback();
    }
    void run()
    {
        FindI2CDevices(_i2cBuses, _busMap);
    }

    const std::vector<fs::path>& _i2cBuses;
    BusMap& _busMap;
    std::function<void(void)> _callback;
};

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
    std::vector<uint8_t>& ret = device->second;

    return ret;
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

    productName = "/xyz/openbmc_project/FruDevice/" + productName;
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
                    (getFRUInfo(static_cast<uint8_t>(busIface.first.first),
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
            // We found something with the same name.  If highest was still -1,
            // it means this new entry will be _0.
            productName += "_";
            productName += std::to_string(++highest);
        }
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface(productName, "xyz.openbmc_project.FruDevice");
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

static bool readBaseboardFRU(std::vector<uint8_t>& baseboardFRU)
{
    // try to read baseboard fru from file
    std::ifstream baseboardFRUFile(BASEBOARD_FRU_LOCATION, std::ios::binary);
    if (baseboardFRUFile.good())
    {
        baseboardFRUFile.seekg(0, std::ios_base::end);
        size_t fileSize = static_cast<size_t>(baseboardFRUFile.tellg());
        baseboardFRU.resize(fileSize);
        baseboardFRUFile.seekg(0, std::ios_base::beg);
        baseboardFRUFile.read(reinterpret_cast<char*>(baseboardFRU.data()),
                              fileSize);
    }
    else
    {
        return false;
    }
    return true;
}

bool writeFRU(uint8_t bus, uint8_t address, const std::vector<uint8_t>& fru)
{
    boost::container::flat_map<std::string, std::string> tmp;
    if (fru.size() > MAX_FRU_SIZE)
    {
        std::cerr << "Invalid fru.size() during writeFRU\n";
        return false;
    }

    // verify legal fru by running it through fru parsing logic
    if (formatFRU(fru, tmp) != resCodes::resOK)
    {
        std::cerr << "Invalid fru format during writeFRU\n";
        return false;
    }
    
    // baseboard fru
    if (bus == 0 && address == 0)
    {
        std::ofstream file(BASEBOARD_FRU_LOCATION, std::ios_base::binary);
        if (!file.good())
        {
            std::cerr << "Error opening file " << BASEBOARD_FRU_LOCATION
                      << "\n";
            throw DBusInternalError();
            return false;
        }
        file.write(reinterpret_cast<const char*>(fru.data()), fru.size());
        return file.good();
    }
    else
    {
        if (hasEepromFile(bus, address))
        {
            auto path = getEepromPath(bus, address);
            int eeprom = open(path.c_str(), O_RDWR | O_CLOEXEC);
            if (eeprom < 0)
            {
                std::cerr << "unable to open i2c device " << path << "\n";
                throw DBusInternalError();
                return false;
            }

            ssize_t writtenBytes = write(eeprom, fru.data(), fru.size());
            if (writtenBytes < 0)
            {
                std::cerr << "unable to write to i2c device " << path << "\n";
                close(eeprom);
                throw DBusInternalError();
                return false;
            }

            close(eeprom);
            return true;
        }

        std::string i2cBus = "/dev/i2c-" + std::to_string(bus);

        int file = open(i2cBus.c_str(), O_RDWR | O_CLOEXEC);
        if (file < 0)
        {
            std::cerr << "unable to open i2c device " << i2cBus << "\n";
            throw DBusInternalError();
            return false;
        }
        if (ioctl(file, I2C_SLAVE_FORCE, address) < 0)
        {
            std::cerr << "unable to set device address\n";
            close(file);
            throw DBusInternalError();
            return false;
        }

        constexpr const size_t RETRY_MAX = 2;
        uint16_t index = 0;
        size_t retries = RETRY_MAX;
        while (index < fru.size())
        {
            if ((index && ((index % (MAX_EEPROM_PAGE_INDEX + 1)) == 0)) &&
                (retries == RETRY_MAX))
            {
                // The 4K EEPROM only uses the A2 and A1 device address bits
                // with the third bit being a memory page address bit.
                if (ioctl(file, I2C_SLAVE_FORCE, ++address) < 0)
                {
                    std::cerr << "unable to set device address\n";
                    close(file);
                    throw DBusInternalError();
                    return false;
                }
            }

            if (i2c_smbus_write_byte_data(file, static_cast<uint8_t>(index),
                                          fru[index]) < 0)
            {
                if (!retries--)
                {
                    std::cerr << "error writing fru: " << strerror(errno)
                              << "\n";
                    close(file);
                    throw DBusInternalError();
                    return false;
                }
            }
            else
            {
                retries = RETRY_MAX;
                index++;
            }
            // most eeproms require 5-10ms between writes
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        close(file);
        return true;
    }
}

void rescanOneBus(
    BusMap& busmap, uint8_t busNum,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    bool dbusCall)
{
    for (auto& [pair, interface] : foundDevices)
    {
        if (pair.first == static_cast<size_t>(busNum))
        {
            objServer.remove_interface(interface);
            foundDevices.erase(pair);
        }
    }

    fs::path busPath = fs::path("/dev/i2c-" + std::to_string(busNum));
    if (!fs::exists(busPath))
    {
        if (dbusCall)
        {
            std::cerr << "Unable to access i2c bus " << static_cast<int>(busNum)
                      << "\n";
            throw std::invalid_argument("Invalid Bus.");
        }
        return;
    }

    std::vector<fs::path> i2cBuses;
    i2cBuses.emplace_back(busPath);

    auto scan = std::make_shared<FindDevicesWithCallback>(
        i2cBuses, busmap, [busNum, &busmap, &dbusInterfaceMap]() {
            for (auto& busIface : dbusInterfaceMap)
            {
                if (busIface.first.first == static_cast<size_t>(busNum))
                {
                    objServer.remove_interface(busIface.second);
                }
            }
            auto found = busmap.find(busNum);
            if (found == busmap.end() || found->second == nullptr)
            {
                return;
            }
            for (auto& device : *(found->second))
            {
                AddFRUObjectToDbus(device.second, dbusInterfaceMap,
                                   static_cast<uint32_t>(busNum), device.first);
            }
        });
    scan->run();
}

void rescanBusses(
    BusMap& busmap,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap)
{
    static boost::asio::deadline_timer timer(io);
    timer.expires_from_now(boost::posix_time::seconds(1));

    // setup an async wait in case we get flooded with requests
    timer.async_wait([&](const boost::system::error_code&) {
        auto devDir = fs::path("/dev/");
        std::vector<fs::path> i2cBuses;

        boost::container::flat_map<size_t, fs::path> busPaths;
        if (!getI2cDevicePaths(devDir, busPaths))
        {
            std::cerr << "unable to find i2c devices\n";
            return;
        }

        for (auto busPath : busPaths)
        {
            i2cBuses.emplace_back(busPath.second);
        }

        busmap.clear();
        for (auto& [pair, interface] : foundDevices)
        {
            objServer.remove_interface(interface);
        }
        foundDevices.clear();

        auto scan =
            std::make_shared<FindDevicesWithCallback>(i2cBuses, busmap, [&]() {
                for (auto& busIface : dbusInterfaceMap)
                {
                    objServer.remove_interface(busIface.second);
                }

                dbusInterfaceMap.clear();
                UNKNOWN_BUS_OBJECT_COUNT = 0;

                // todo, get this from a more sensable place
                std::vector<uint8_t> baseboardFRU;
                if (readBaseboardFRU(baseboardFRU))
                {
                    boost::container::flat_map<int, std::vector<uint8_t>>
                        baseboardDev;
                    baseboardDev.emplace(0, baseboardFRU);
                    busmap[0] = std::make_shared<DeviceMap>(baseboardDev);
                }
                for (auto& devicemap : busmap)
                {
                    for (auto& device : *devicemap.second)
                    {
                        AddFRUObjectToDbus(device.second, dbusInterfaceMap,
                                           devicemap.first, device.first);
                    }
                }
            });
        scan->run();
    });
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

    if (!writeFRU(static_cast<uint8_t>(bus), static_cast<uint8_t>(address),
                  fruData))
    {
        return false;
    }

    // Rescan the bus so that GetRawFru dbus-call fetches updated values
    rescanBusses(busMap, dbusInterfaceMap);
    return true;
}

int main()
{
    auto devDir = fs::path("/dev/");
    auto matchString = std::string(R"(i2c-\d+$)");
    std::vector<fs::path> i2cBuses;

    if (!findFiles(devDir, matchString, i2cBuses))
    {
        std::cerr << "unable to find i2c devices\n";
        return 1;
    }

    // check for and load blacklist with initial buses.
    loadBlacklist(blacklistPath);

    systemBus->request_name("xyz.openbmc_project.FruDevice");

    // this is a map with keys of pair(bus number, address) and values of
    // the object on dbus
    boost::container::flat_map<std::pair<size_t, size_t>,
                               std::shared_ptr<sdbusplus::asio::dbus_interface>>
        dbusInterfaceMap;

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface("/xyz/openbmc_project/FruDevice",
                                "xyz.openbmc_project.FruDeviceManager");

    iface->register_method("ReScan",
                           [&]() { rescanBusses(busMap, dbusInterfaceMap); });

    iface->register_method("ReScanBus", [&](uint8_t bus) {
        rescanOneBus(busMap, bus, dbusInterfaceMap, true);
    });

    iface->register_method("GetRawFru", getFRUInfo);

    iface->register_method("WriteFru", [&](const uint8_t bus,
                                           const uint8_t address,
                                           const std::vector<uint8_t>& data) {
        if (!writeFRU(bus, address, data))
        {
            throw std::invalid_argument("Invalid Arguments.");
            return;
        }
        // schedule rescan on success
        rescanBusses(busMap, dbusInterfaceMap);
    });
    iface->initialize();

    std::function<void(sdbusplus::message::message & message)> eventHandler =
        [&](sdbusplus::message::message& message) {
            std::string objectName;
            boost::container::flat_map<
                std::string,
                std::variant<std::string, bool, int64_t, uint64_t, double>>
                values;
            message.read(objectName, values);
            auto findState = values.find("CurrentHostState");
            if (findState != values.end())
            {
                powerIsOn = boost::ends_with(
                    std::get<std::string>(findState->second), "Running");
            }

            if (powerIsOn)
            {
                rescanBusses(busMap, dbusInterfaceMap);
            }
        };

    sdbusplus::bus::match::match powerMatch = sdbusplus::bus::match::match(
        static_cast<sdbusplus::bus::bus&>(*systemBus),
        "type='signal',interface='org.freedesktop.DBus.Properties',path='/xyz/"
        "openbmc_project/state/"
        "host0',arg0='xyz.openbmc_project.State.Host'",
        eventHandler);

    int fd = inotify_init();
    inotify_add_watch(fd, I2C_DEV_LOCATION,
                      IN_CREATE | IN_MOVED_TO | IN_DELETE);
    std::array<char, 4096> readBuffer;
    std::string pendingBuffer;
    // monitor for new i2c devices
    boost::asio::posix::stream_descriptor dirWatch(io, fd);
    std::function<void(const boost::system::error_code, std::size_t)>
        watchI2cBusses = [&](const boost::system::error_code& ec,
                             std::size_t bytes_transferred) {
            if (ec)
            {
                std::cout << "Callback Error " << ec << "\n";
                return;
            }
            pendingBuffer += std::string(readBuffer.data(), bytes_transferred);
            while (pendingBuffer.size() > sizeof(inotify_event))
            {
                const inotify_event* iEvent =
                    reinterpret_cast<const inotify_event*>(
                        pendingBuffer.data());
                switch (iEvent->mask)
                {
                    case IN_CREATE:
                    case IN_MOVED_TO:
                    case IN_DELETE:
                        std::string name(iEvent->name);
                        if (boost::starts_with(name, "i2c"))
                        {
                            int bus = busStrToInt(name);
                            if (bus < 0)
                            {
                                std::cerr << "Could not parse bus " << name
                                          << "\n";
                                continue;
                            }
                            rescanOneBus(busMap, static_cast<uint8_t>(bus),
                                         dbusInterfaceMap, false);
                        }
                }

                pendingBuffer.erase(0, sizeof(inotify_event) + iEvent->len);
            }

            dirWatch.async_read_some(boost::asio::buffer(readBuffer),
                                     watchI2cBusses);
        };

    dirWatch.async_read_some(boost::asio::buffer(readBuffer), watchI2cBusses);
    // run the initial scan
    rescanBusses(busMap, dbusInterfaceMap);

    io.run();
    return 0;
}
