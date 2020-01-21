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

#include "Utils.hpp"

#include <errno.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>

#include <array>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/container/flat_map.hpp>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <regex>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <variant>
#include <vector>

extern "C" {
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

static constexpr std::array<const char*, 5> FRU_AREAS = {
    "INTERNAL", "CHASSIS", "BOARD", "PRODUCT", "MULTIRECORD"};
const static std::regex NON_ASCII_REGEX("[^\x01-\x7f]");
using DeviceMap = boost::container::flat_map<int, std::vector<char>>;
using BusMap = boost::container::flat_map<int, std::shared_ptr<DeviceMap>>;

static std::set<size_t> busBlacklist;
struct FindDevicesWithCallback;

static BusMap busMap;
static size_t rescanBusNum;

static boost::container::flat_map<
    std::pair<size_t, size_t>, std::shared_ptr<sdbusplus::asio::dbus_interface>>
    foundDevices;

static boost::container::flat_map<size_t, std::set<size_t>> failedAddresses;

boost::asio::io_service io;
auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
auto objServer = sdbusplus::asio::object_server(systemBus);

bool validateHeader(const std::array<uint8_t, I2C_SMBUS_BLOCK_MAX>& blockData);

using ReadBlockFunc = std::function<int64_t(int flag, int file, uint16_t offset,
                                            uint8_t length, uint8_t* outBuf)>;

// Read and validate FRU contents, given the flag required for raw i2c, the open
// file handle, a read method, and a helper string for failures.
std::vector<char> readFruContents(int flag, int file, ReadBlockFunc readBlock,
                                  const std::string& errorHelp)
{
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;

    if (readBlock(flag, file, 0x0, 0x8, blockData.data()) < 0)
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

    std::vector<char> device;
    device.insert(device.end(), blockData.begin(), blockData.begin() + 8);

    bool hasMultiRecords = false;
    int fruLength = 0;
    for (size_t jj = 1; jj <= FRU_AREAS.size(); jj++)
    {
        // Offset value can be 255.
        int areaOffset = static_cast<uint8_t>(device[jj]);
        if (areaOffset == 0)
        {
            continue;
        }

        // MultiRecords are different.  jj is not tracking section, it's walking
        // the common header.
        if (std::string(FRU_AREAS[jj - 1]) == std::string("MULTIRECORD"))
        {
            hasMultiRecords = true;
            break;
        }

        areaOffset *= 8;

        if (readBlock(flag, file, static_cast<uint16_t>(areaOffset), 0x2,
                      blockData.data()) < 0)
        {
            std::cerr << "failed to read " << errorHelp << "\n";
            return {};
        }

        // Ignore data type (blockData is already unsigned).
        int length = blockData[1] * 8;
        areaOffset += length;
        fruLength = (areaOffset > fruLength) ? areaOffset : fruLength;
    }

    if (hasMultiRecords)
    {
        // device[area count] is the index to the last area because the 0th
        // entry is not an offset in the common header.
        int areaOffset = static_cast<uint8_t>(device[FRU_AREAS.size()]);
        areaOffset *= 8;

        // the multi-area record header is 5 bytes long.
        constexpr int multiRecordHeaderSize = 5;
        constexpr int multiRecordEndOfList = 0x80;

        // Sanity hard-limit to 64KB.
        while (areaOffset < std::numeric_limits<uint16_t>::max())
        {
            // In multi-area, the area offset points to the 0th record, each
            // record has 3 bytes of the header we care about.
            if (readBlock(flag, file, static_cast<uint16_t>(areaOffset), 0x3,
                          blockData.data()) < 0)
            {
                std::cerr << "failed to read " << errorHelp << "\n";
                return {};
            }

            // Ok, let's check the record length, which is in bytes (unsigned,
            // up to 255, so blockData should hold uint8_t not char)
            int recordLength = blockData[2];
            areaOffset += (recordLength + multiRecordHeaderSize);
            fruLength = (areaOffset > fruLength) ? areaOffset : fruLength;

            // If this is the end of the list bail.
            if ((blockData[1] & multiRecordEndOfList))
            {
                break;
            }
        }
    }

    // You already copied these first 8 bytes (the ipmi fru header size)
    fruLength -= 8;

    int readOffset = 8;

    while (fruLength > 0)
    {
        int requestLength = std::min(I2C_SMBUS_BLOCK_MAX, fruLength);

        if (readBlock(flag, file, static_cast<uint16_t>(readOffset),
                      static_cast<uint8_t>(requestLength),
                      blockData.data()) < 0)
        {
            std::cerr << "failed to read " << errorHelp << "\n";
            return {};
        }

        device.insert(device.end(), blockData.begin(),
                      blockData.begin() + requestLength);

        readOffset += requestLength;
        fruLength -= requestLength;
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

static bool isMuxBus(size_t bus)
{
    return is_symlink(std::filesystem::path(
        "/sys/bus/i2c/devices/i2c-" + std::to_string(bus) + "/mux_device"));
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

static int64_t readBlockData(int flag, int file, uint16_t offset, uint8_t len,
                             uint8_t* buf)
{
    uint8_t lowAddr = static_cast<uint8_t>(offset);
    uint8_t highAddr = static_cast<uint8_t>(offset >> 8);

    if (flag == 0)
    {
        return i2c_smbus_read_i2c_block_data(file, lowAddr, len, buf);
    }

    /* This is for 16 bit addressing EEPROM device. First an offset
     * needs to be written before read data from a offset
     */
    int ret = i2c_smbus_write_byte_data(file, 0, lowAddr);
    if (ret < 0)
    {
        return ret;
    }

    return i2c_smbus_read_i2c_block_data(file, highAddr, len, buf);
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
static std::vector<char> processEeprom(int bus, int address)
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
    std::vector<char> device =
        readFruContents(0, file, readFromEeprom, errorMessage);

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

        std::vector<char> device = processEeprom(i2cBus, address);
        if (!device.empty())
        {
            devices->emplace(address, device);
        }
    }

    return foundList;
}

int getBusFrus(int file, int first, int last, int bus,
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

        for (int ii = first; ii <= last; ii++)
        {
            if (skipList.find(ii) != skipList.end())
            {
                continue;
            }

            // Set slave address
            if (ioctl(file, I2C_SLAVE_FORCE, ii) < 0)
            {
                std::cerr << "device at bus " << bus << " register " << ii
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
                failedItems.insert(ii);
                continue;
            }

            std::string errorMessage =
                "bus " + std::to_string(bus) + " address " + std::to_string(ii);
            std::vector<char> device =
                readFruContents(flag, file, readBlockData, errorMessage);
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
        busBlacklist.insert(bus);
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
        auto busnum = i2cBus.string();
        auto lastDash = busnum.rfind(std::string("-"));
        // delete everything before dash inclusive
        if (lastDash != std::string::npos)
        {
            busnum.erase(0, lastDash + 1);
        }

        auto bus = std::stoi(busnum);
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
        getBusFrus(file, 0x03, 0x77, bus, device);

        if (DEBUG)
        {
            std::cerr << "Done scanning bus " << bus << "\n";
        }
    }
}

// this class allows an async response after all i2c devices are discovered
struct FindDevicesWithCallback
    : std::enable_shared_from_this<FindDevicesWithCallback>
{
    FindDevicesWithCallback(const std::vector<fs::path>& i2cBuses,
                            BusMap& busmap,
                            std::function<void(void)>&& callback) :
        _i2cBuses(i2cBuses),
        _busMap(busmap), _callback(std::move(callback))
    {
    }
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

static const std::tm intelEpoch(void)
{
    std::tm val = {};
    val.tm_year = 1996 - 1900;
    return val;
}

bool formatFru(const std::vector<char>& fruBytes,
               boost::container::flat_map<std::string, std::string>& result)
{
    static const std::vector<const char*> CHASSIS_FRU_AREAS = {
        "PART_NUMBER", "SERIAL_NUMBER", "INFO_AM1", "INFO_AM2"};

    static const std::vector<const char*> BOARD_FRU_AREAS = {
        "MANUFACTURER",   "PRODUCT_NAME", "SERIAL_NUMBER", "PART_NUMBER",
        "FRU_VERSION_ID", "INFO_AM1",     "INFO_AM2"};

    static const std::vector<const char*> PRODUCT_FRU_AREAS = {
        "MANUFACTURER",   "PRODUCT_NAME",  "PART_NUMBER",
        "VERSION",        "SERIAL_NUMBER", "ASSET_TAG",
        "FRU_VERSION_ID", "INFO_AM1",      "INFO_AM2"};

    if (fruBytes.size() <= 8)
    {
        return false;
    }
    std::vector<char>::const_iterator fruAreaOffsetField = fruBytes.begin();
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
                if (fruBytesIter >= fruBytes.end())
                {
                    return false;
                }

                /* Checking for last byte C1 to indicate that no more
                 * field to be read */
                if (static_cast<uint8_t>(*fruBytesIter) == 0xC1)
                {
                    break;
                }

                size_t length = *fruBytesIter & 0x3f;
                fruBytesIter += 1;

                if (fruBytesIter >= fruBytes.end())
                {
                    return false;
                }
                std::string value(fruBytesIter, fruBytesIter + length);

                // Strip non null characters from the end
                value.erase(std::find_if(value.rbegin(), value.rend(),
                                         [](char ch) { return ch != 0; })
                                .base(),
                            value.end());

                result[area + "_" + field] = std::move(value);

                fruBytesIter += length;
                if (fruBytesIter >= fruBytes.end())
                {
                    std::cerr << "Warning Fru Length Mismatch:\n    ";
                    for (auto& c : fruBytes)
                    {
                        std::cerr << c;
                    }
                    std::cerr << "\n";
                    if (DEBUG)
                    {
                        for (auto& keyPair : result)
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
    std::vector<uint8_t>& ret =
        reinterpret_cast<std::vector<uint8_t>&>(device->second);

    return ret;
}

void AddFruObjectToDbus(
    std::vector<char>& device,
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
        objServer.add_interface(productName, "xyz.openbmc_project.FruDevice");
    dbusInterfaceMap[std::pair<size_t, size_t>(bus, address)] = iface;

    for (auto& property : formattedFru)
    {

        std::regex_replace(property.second.begin(), property.second.begin(),
                           property.second.end(), NON_ASCII_REGEX, "_");
        if (property.second.empty())
        {
            continue;
        }
        std::string key =
            std::regex_replace(property.first, NON_ASCII_REGEX, "_");
        if (!iface->register_property(key, property.second + '\0'))
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

static bool readBaseboardFru(std::vector<char>& baseboardFru)
{
    // try to read baseboard fru from file
    std::ifstream baseboardFruFile(BASEBOARD_FRU_LOCATION, std::ios::binary);
    if (baseboardFruFile.good())
    {
        baseboardFruFile.seekg(0, std::ios_base::end);
        size_t fileSize = static_cast<size_t>(baseboardFruFile.tellg());
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

bool writeFru(uint8_t bus, uint8_t address, const std::vector<uint8_t>& fru)
{
    boost::container::flat_map<std::string, std::string> tmp;
    if (fru.size() > MAX_FRU_SIZE)
    {
        std::cerr << "Invalid fru.size() during writeFru\n";
        return false;
    }
    // verify legal fru by running it through fru parsing logic
    if (!formatFru(reinterpret_cast<const std::vector<char>&>(fru), tmp))
    {
        std::cerr << "Invalid fru format during writeFru\n";
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
    BusMap& busmap, uint32_t& busNum,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap)
{
    rescanBusNum = busNum;
    static boost::asio::deadline_timer timer(io);
    timer.expires_from_now(boost::posix_time::seconds(1));

    // setup an async wait in case we get flooded with requests
    timer.async_wait([&](const boost::system::error_code&) {
        fs::path busPath = fs::path("/dev/i2c-" + std::to_string(rescanBusNum));
        if (!fs::exists(busPath))
        {
            std::cerr << "Unable to access i2c bus " << rescanBusNum << "\n";
            return;
        }

        std::vector<fs::path> i2cBuses;
        i2cBuses.emplace_back(busPath);

        for (auto& [pair, interface] : foundDevices)
        {
            if (pair.first == rescanBusNum)
            {
                objServer.remove_interface(interface);
                foundDevices.erase(pair);
            }
        }
        auto scan =
            std::make_shared<FindDevicesWithCallback>(i2cBuses, busmap, [&]() {
                for (auto& busIface : dbusInterfaceMap)
                {
                    if (busIface.first.first == rescanBusNum)
                    {
                        objServer.remove_interface(busIface.second);
                    }
                }
                auto& devicemap = busmap[rescanBusNum];
                for (auto& device : *devicemap)
                {
                    AddFruObjectToDbus(device.second, dbusInterfaceMap,
                                       rescanBusNum, device.first);
                }
            });
        scan->run();
    });
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
                std::vector<char> baseboardFru;
                if (readBaseboardFru(baseboardFru))
                {
                    boost::container::flat_map<int, std::vector<char>>
                        baseboardDev;
                    baseboardDev.emplace(0, baseboardFru);
                    busmap[0] = std::make_shared<DeviceMap>(baseboardDev);
                }
                for (auto& devicemap : busmap)
                {
                    for (auto& device : *devicemap.second)
                    {
                        AddFruObjectToDbus(device.second, dbusInterfaceMap,
                                           devicemap.first, device.first);
                    }
                }
            });
        scan->run();
    });
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

    iface->register_method("ReScanBus", [&](uint32_t bus) {
        rescanOneBus(busMap, bus, dbusInterfaceMap);
    });

    iface->register_method("GetRawFru", getFruInfo);

    iface->register_method("WriteFru", [&](const uint8_t bus,
                                           const uint8_t address,
                                           const std::vector<uint8_t>& data) {
        if (!writeFru(bus, address, data))
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
            bool on = false;
            if (findState != values.end())
            {
                on = boost::ends_with(std::get<std::string>(findState->second),
                                      "Running");
            }

            if (on)
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
            bool devChange = false;
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
                        if (boost::starts_with(std::string(iEvent->name),
                                               "i2c"))
                        {
                            devChange = true;
                        }
                }

                pendingBuffer.erase(0, sizeof(inotify_event) + iEvent->len);
            }
            if (devChange)
            {
                rescanBusses(busMap, dbusInterfaceMap);
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
