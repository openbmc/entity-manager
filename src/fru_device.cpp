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
/// \file fru_device.cpp

#include "fru_utils.hpp"
#include "utils.hpp"

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
#include <cerrno>
#include <charconv>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
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
static constexpr bool debug = false;
constexpr size_t maxFruSize = 512;
constexpr size_t maxEepromPageIndex = 255;
constexpr size_t busTimeoutSeconds = 5;

constexpr const char* blacklistPath = PACKAGE_DIR "blacklist.json";

const static constexpr char* baseboardFruLocation =
    "/etc/fru/baseboard.fru.bin";

const static constexpr char* i2CDevLocation = "/dev";

static std::set<size_t> busBlacklist;
struct FindDevicesWithCallback;

static boost::container::flat_map<
    std::pair<size_t, size_t>, std::shared_ptr<sdbusplus::asio::dbus_interface>>
    foundDevices;

static boost::container::flat_map<size_t, std::set<size_t>> failedAddresses;

boost::asio::io_service io;

bool updateFRUProperty(
    const std::string& updatePropertyReq, uint32_t bus, uint32_t address,
    const std::string& propertyName,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    size_t& unknownBusObjectCount, const bool& powerIsOn,
    sdbusplus::asio::object_server& objServer,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus);

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

static int64_t readFromEeprom(int fd, off_t offset, size_t len, uint8_t* buf)
{
    auto result = lseek(fd, offset, SEEK_SET);
    if (result < 0)
    {
        std::cerr << "failed to seek\n";
        return -1;
    }

    return read(fd, buf, len);
}

static int busStrToInt(const std::string_view busName)
{
    auto findBus = busName.rfind('-');
    if (findBus == std::string::npos)
    {
        return -1;
    }
    std::string_view num = busName.substr(findBus + 1);
    int val = 0;
    std::from_chars(num.data(), num.data() + num.size(), val);
    return val;
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
    auto findBus = filename.find('-');
    if (findBus == std::string::npos)
    {
        return -1;
    }
    return std::stoi(filename.substr(0, findBus));
}

bool isMuxBus(size_t bus)
{
    return is_symlink(std::filesystem::path(
        "/sys/bus/i2c/devices/i2c-" + std::to_string(bus) + "/mux_device"));
}

static void makeProbeInterface(size_t bus, size_t address,
                               sdbusplus::asio::object_server& objServer)
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

static std::optional<bool> isDevice16Bit(int file)
{
    // Set the higher data word address bits to 0. It's safe on 8-bit addressing
    // EEPROMs because it doesn't write any actual data.
    int ret = i2c_smbus_write_byte(file, 0);
    if (ret < 0)
    {
        return std::nullopt;
    }

    /* Get first byte */
    int byte1 = i2c_smbus_read_byte_data(file, 0);
    if (byte1 < 0)
    {
        return std::nullopt;
    }
    /* Read 7 more bytes, it will read same first byte in case of
     * 8 bit but it will read next byte in case of 16 bit
     */
    for (int i = 0; i < 7; i++)
    {
        int byte2 = i2c_smbus_read_byte_data(file, 0);
        if (byte2 < 0)
        {
            return std::nullopt;
        }
        if (byte2 != byte1)
        {
            return true;
        }
    }
    return false;
}

// Issue an I2C transaction to first write to_slave_buf_len bytes,then read
// from_slave_buf_len bytes.
static int i2cSmbusWriteThenRead(int file, uint16_t address,
                                 uint8_t* toSlaveBuf, uint8_t toSlaveBufLen,
                                 uint8_t* fromSlaveBuf, uint8_t fromSlaveBufLen)
{
    if (toSlaveBuf == nullptr || toSlaveBufLen == 0 ||
        fromSlaveBuf == nullptr || fromSlaveBufLen == 0)
    {
        return -1;
    }

    constexpr size_t smbusWriteThenReadMsgCount = 2;
    std::array<struct i2c_msg, smbusWriteThenReadMsgCount> msgs{};
    struct i2c_rdwr_ioctl_data rdwr
    {};

    msgs[0].addr = address;
    msgs[0].flags = 0;
    msgs[0].len = toSlaveBufLen;
    msgs[0].buf = toSlaveBuf;
    msgs[1].addr = address;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = fromSlaveBufLen;
    msgs[1].buf = fromSlaveBuf;

    rdwr.msgs = msgs.data();
    rdwr.nmsgs = msgs.size();

    int ret = ioctl(file, I2C_RDWR, &rdwr);

    return (ret == static_cast<int>(msgs.size())) ? msgs[1].len : -1;
}

static int64_t readBlockData(bool is16bit, int file, uint16_t address,
                             off_t offset, size_t len, uint8_t* buf)
{
    if (!is16bit)
    {
        return i2c_smbus_read_i2c_block_data(file, static_cast<uint8_t>(offset),
                                             len, buf);
    }

    offset = htobe16(offset);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    uint8_t* u8Offset = reinterpret_cast<uint8_t*>(&offset);
    return i2cSmbusWriteThenRead(file, address, u8Offset, 2, buf, len);
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
    auto readFunc = [file](off_t offset, size_t length, uint8_t* outbuf) {
        return readFromEeprom(file, offset, length, outbuf);
    };
    FRUReader reader(std::move(readFunc));
    std::vector<uint8_t> device = readFRUContents(reader, errorMessage);

    close(file);
    return device;
}

std::set<int> findI2CEeproms(int i2cBus,
                             const std::shared_ptr<DeviceMap>& devices)
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

        std::size_t ignored = 0;
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
               std::shared_ptr<DeviceMap> devices, const bool& powerIsOn,
               sdbusplus::asio::object_server& objServer)
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
            if (i2c_smbus_read_byte(file) < 0)
            {
                continue;
            }

            if (debug)
            {
                std::cout << "something at bus " << bus << " addr " << ii
                          << "\n";
            }

            makeProbeInterface(bus, ii, objServer);

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
            std::optional<bool> is16Bit = isDevice16Bit(file);
            if (!is16Bit.has_value())
            {
                std::cerr << "failed to read bus " << bus << " address " << ii
                          << "\n";
                if (powerIsOn)
                {
                    failedItems.insert(ii);
                }
                continue;
            }
            bool is16BitBool{*is16Bit};

            auto readFunc = [is16BitBool, file, ii](off_t offset, size_t length,
                                                    uint8_t* outbuf) {
                return readBlockData(is16BitBool, file, ii, offset, length,
                                     outbuf);
            };
            FRUReader reader(std::move(readFunc));
            std::string errorMessage =
                "bus " + std::to_string(bus) + " address " + std::to_string(ii);
            std::vector<uint8_t> device = readFRUContents(reader, errorMessage);
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
}

static void findI2CDevices(const std::vector<fs::path>& i2cBuses,
                           BusMap& busmap, const bool& powerIsOn,
                           sdbusplus::asio::object_server& objServer)
{
    for (const auto& i2cBus : i2cBuses)
    {
        int bus = busStrToInt(i2cBus.string());

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
            close(file);
            continue;
        }
        if (((funcs & I2C_FUNC_SMBUS_READ_BYTE) == 0U) ||
            ((I2C_FUNC_SMBUS_READ_I2C_BLOCK) == 0))
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
        if (debug)
        {
            std::cerr << "Scanning bus " << bus << "\n";
        }

        // fd is closed in this function in case the bus locks up
        getBusFRUs(file, 0x03, 0x77, bus, device, powerIsOn, objServer);

        if (debug)
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
                            BusMap& busmap, const bool& powerIsOn,
                            sdbusplus::asio::object_server& objServer,
                            std::function<void(void)>&& callback) :
        _i2cBuses(i2cBuses),
        _busMap(busmap), _powerIsOn(powerIsOn), _objServer(objServer),
        _callback(std::move(callback))
    {}
    ~FindDevicesWithCallback()
    {
        _callback();
    }
    void run()
    {
        findI2CDevices(_i2cBuses, _busMap, _powerIsOn, _objServer);
    }

    const std::vector<fs::path>& _i2cBuses;
    BusMap& _busMap;
    const bool& _powerIsOn;
    sdbusplus::asio::object_server& _objServer;
    std::function<void(void)> _callback;
};

void addFruObjectToDbus(
    std::vector<uint8_t>& device,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    uint32_t bus, uint32_t address, size_t& unknownBusObjectCount,
    const bool& powerIsOn, sdbusplus::asio::object_server& objServer,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus)
{
    boost::container::flat_map<std::string, std::string> formattedFRU;
    resCodes res = formatIPMIFRU(device, formattedFRU);
    if (res == resCodes::resErr)
    {
        std::cerr << "failed to parse FRU for device at bus " << bus
                  << " address " << address << "\n";
        return;
    }
    if (res == resCodes::resWarn)
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
        productName = "UNKNOWN" + std::to_string(unknownBusObjectCount);
        unknownBusObjectCount++;
    }

    searchFRUDbusObjects(bus, address, dbusInterfaceMap, productName);

    productName = "/xyz/openbmc_project/FruDevice/" + productName;

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface(productName, "xyz.openbmc_project.FruDevice");
    dbusInterfaceMap[std::pair<size_t, size_t>(bus, address)] = iface;

    for (auto& property : formattedFRU)
    {

        std::regex_replace(property.second.begin(), property.second.begin(),
                           property.second.end(), nonAsciiRegex, "_");
        if (property.second.empty() && property.first != "PRODUCT_ASSET_TAG")
        {
            continue;
        }
        std::string key =
            std::regex_replace(property.first, nonAsciiRegex, "_");

        if (property.first == "PRODUCT_ASSET_TAG")
        {
            std::string propertyName = property.first;
            iface->register_property(
                key, property.second + '\0',
                [bus, address, propertyName, &dbusInterfaceMap,
                 &unknownBusObjectCount, &powerIsOn, &objServer,
                 &systemBus](const std::string& req, std::string& resp) {
                    if (strcmp(req.c_str(), resp.c_str()) != 0)
                    {
                        // call the method which will update
                        if (updateFRUProperty(req, bus, address, propertyName,
                                              dbusInterfaceMap,
                                              unknownBusObjectCount, powerIsOn,
                                              objServer, systemBus))
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
        if (debug)
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
    std::ifstream baseboardFRUFile(baseboardFruLocation, std::ios::binary);
    if (baseboardFRUFile.good())
    {
        baseboardFRUFile.seekg(0, std::ios_base::end);
        size_t fileSize = static_cast<size_t>(baseboardFRUFile.tellg());
        baseboardFRU.resize(fileSize);
        baseboardFRUFile.seekg(0, std::ios_base::beg);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        char* charOffset = reinterpret_cast<char*>(baseboardFRU.data());
        baseboardFRUFile.read(charOffset, fileSize);
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
    if (fru.size() > maxFruSize)
    {
        std::cerr << "Invalid fru.size() during writeFRU\n";
        return false;
    }
    // verify legal fru by running it through fru parsing logic
    if (formatIPMIFRU(fru, tmp) != resCodes::resOK)
    {
        std::cerr << "Invalid fru format during writeFRU\n";
        return false;
    }
    // baseboard fru
    if (bus == 0 && address == 0)
    {
        std::ofstream file(baseboardFruLocation, std::ios_base::binary);
        if (!file.good())
        {
            std::cerr << "Error opening file " << baseboardFruLocation << "\n";
            throw DBusInternalError();
            return false;
        }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const char* charOffset = reinterpret_cast<const char*>(fru.data());
        file.write(charOffset, fru.size());
        return file.good();
    }

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

    constexpr const size_t retryMax = 2;
    uint16_t index = 0;
    size_t retries = retryMax;
    while (index < fru.size())
    {
        if (((index != 0U) && ((index % (maxEepromPageIndex + 1)) == 0)) &&
            (retries == retryMax))
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
            if ((retries--) == 0U)
            {
                std::cerr << "error writing fru: " << strerror(errno) << "\n";
                close(file);
                throw DBusInternalError();
                return false;
            }
        }
        else
        {
            retries = retryMax;
            index++;
        }
        // most eeproms require 5-10ms between writes
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    close(file);
    return true;
}

void rescanOneBus(
    BusMap& busmap, uint8_t busNum,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    bool dbusCall, size_t& unknownBusObjectCount, const bool& powerIsOn,
    sdbusplus::asio::object_server& objServer,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus)
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
        i2cBuses, busmap, powerIsOn, objServer,
        [busNum, &busmap, &dbusInterfaceMap, &unknownBusObjectCount, &powerIsOn,
         &objServer, &systemBus]() {
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
                addFruObjectToDbus(device.second, dbusInterfaceMap,
                                   static_cast<uint32_t>(busNum), device.first,
                                   unknownBusObjectCount, powerIsOn, objServer,
                                   systemBus);
            }
        });
    scan->run();
}

void rescanBusses(
    BusMap& busmap,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    size_t& unknownBusObjectCount, const bool& powerIsOn,
    sdbusplus::asio::object_server& objServer,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus)
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

        for (const auto& busPath : busPaths)
        {
            i2cBuses.emplace_back(busPath.second);
        }

        busmap.clear();
        for (auto& [pair, interface] : foundDevices)
        {
            objServer.remove_interface(interface);
        }
        foundDevices.clear();

        auto scan = std::make_shared<FindDevicesWithCallback>(
            i2cBuses, busmap, powerIsOn, objServer, [&]() {
                for (auto& busIface : dbusInterfaceMap)
                {
                    objServer.remove_interface(busIface.second);
                }

                dbusInterfaceMap.clear();
                unknownBusObjectCount = 0;

                // todo, get this from a more sensable place
                std::vector<uint8_t> baseboardFRU;
                if (readBaseboardFRU(baseboardFRU))
                {
                    // If no device on i2c bus 0, the insertion will happen.
                    auto bus0 =
                        busmap.try_emplace(0, std::make_shared<DeviceMap>());
                    bus0.first->second->emplace(0, baseboardFRU);
                }
                for (auto& devicemap : busmap)
                {
                    for (auto& device : *devicemap.second)
                    {
                        addFruObjectToDbus(device.second, dbusInterfaceMap,
                                           devicemap.first, device.first,
                                           unknownBusObjectCount, powerIsOn,
                                           objServer, systemBus);
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
    const std::string& propertyName,
    boost::container::flat_map<
        std::pair<size_t, size_t>,
        std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
    size_t& unknownBusObjectCount, const bool& powerIsOn,
    sdbusplus::asio::object_server& objServer,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus)
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
    catch (const std::invalid_argument& e)
    {
        std::cerr << "Failure getting FRU Info" << e.what() << "\n";
        return false;
    }

    if (fruData.empty())
    {
        return false;
    }

    const std::vector<std::string>* fruAreaFieldNames = nullptr;

    uint8_t fruAreaOffsetFieldValue = 0;
    size_t offset = 0;
    std::string areaName = propertyName.substr(0, propertyName.find('_'));
    std::string propertyNamePrefix = areaName + "_";
    auto it = std::find(fruAreaNames.begin(), fruAreaNames.end(), areaName);
    if (it == fruAreaNames.end())
    {
        std::cerr << "Can't parse area name for property " << propertyName
                  << " \n";
        return false;
    }
    fruAreas fruAreaToUpdate = static_cast<fruAreas>(it - fruAreaNames.begin());
    fruAreaOffsetFieldValue =
        fruData[getHeaderAreaFieldOffset(fruAreaToUpdate)];
    switch (fruAreaToUpdate)
    {
        case fruAreas::fruAreaChassis:
            offset = 3; // chassis part number offset. Skip fixed first 3 bytes
            fruAreaFieldNames = &chassisFruAreas;
            break;
        case fruAreas::fruAreaBoard:
            offset = 6; // board manufacturer offset. Skip fixed first 6 bytes
            fruAreaFieldNames = &boardFruAreas;
            break;
        case fruAreas::fruAreaProduct:
            // Manufacturer name offset. Skip fixed first 3 product fru bytes
            // i.e. version, area length and language code
            offset = 3;
            fruAreaFieldNames = &productFruAreas;
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
    size_t skipToFRUUpdateField = 0;
    ssize_t fieldLength = 0;

    bool found = false;
    for (const auto& field : *fruAreaFieldNames)
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
        std::size_t pos = propertyName.find(fruCustomFieldName);
        if (pos == std::string::npos)
        {
            std::cerr << "PropertyName doesn't exist in FRU Area Vectors: "
                      << propertyName << "\n";
            return false;
        }
        std::string fieldNumStr =
            propertyName.substr(pos + fruCustomFieldName.length());
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
    size_t fruUpdateFieldLoc = fruDataIter;

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
    if (nextFRUAreaLoc != 0U)
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
    rescanBusses(busMap, dbusInterfaceMap, unknownBusObjectCount, powerIsOn,
                 objServer, systemBus);
    return true;
}

int main()
{
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);

    static size_t unknownBusObjectCount = 0;
    static bool powerIsOn = false;
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

    iface->register_method("ReScan", [&]() {
        rescanBusses(busMap, dbusInterfaceMap, unknownBusObjectCount, powerIsOn,
                     objServer, systemBus);
    });

    iface->register_method("ReScanBus", [&](uint8_t bus) {
        rescanOneBus(busMap, bus, dbusInterfaceMap, true, unknownBusObjectCount,
                     powerIsOn, objServer, systemBus);
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
        rescanBusses(busMap, dbusInterfaceMap, unknownBusObjectCount, powerIsOn,
                     objServer, systemBus);
    });
    iface->initialize();

    std::function<void(sdbusplus::message_t & message)> eventHandler =
        [&](sdbusplus::message_t& message) {
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
                rescanBusses(busMap, dbusInterfaceMap, unknownBusObjectCount,
                             powerIsOn, objServer, systemBus);
            }
        };

    sdbusplus::bus::match_t powerMatch = sdbusplus::bus::match_t(
        static_cast<sdbusplus::bus_t&>(*systemBus),
        "type='signal',interface='org.freedesktop.DBus.Properties',path='/xyz/"
        "openbmc_project/state/"
        "host0',arg0='xyz.openbmc_project.State.Host'",
        eventHandler);

    int fd = inotify_init();
    inotify_add_watch(fd, i2CDevLocation, IN_CREATE | IN_MOVED_TO | IN_DELETE);
    std::array<char, 4096> readBuffer{};
    // monitor for new i2c devices
    boost::asio::posix::stream_descriptor dirWatch(io, fd);
    std::function<void(const boost::system::error_code, std::size_t)>
        watchI2cBusses = [&](const boost::system::error_code& ec,
                             std::size_t bytesTransferred) {
            if (ec)
            {
                std::cout << "Callback Error " << ec << "\n";
                return;
            }
            size_t index = 0;
            while ((index + sizeof(inotify_event)) <= bytesTransferred)
            {
                const char* p = &readBuffer[index];
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
                const auto* iEvent = reinterpret_cast<const inotify_event*>(p);
                switch (iEvent->mask)
                {
                    case IN_CREATE:
                    case IN_MOVED_TO:
                    case IN_DELETE:
                        std::string_view name(&iEvent->name[0], iEvent->len);
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
                                         dbusInterfaceMap, false,
                                         unknownBusObjectCount, powerIsOn,
                                         objServer, systemBus);
                        }
                }
                index += sizeof(inotify_event) + iEvent->len;
            }

            dirWatch.async_read_some(boost::asio::buffer(readBuffer),
                                     watchI2cBusses);
        };

    dirWatch.async_read_some(boost::asio::buffer(readBuffer), watchI2cBusses);
    // run the initial scan
    rescanBusses(busMap, dbusInterfaceMap, unknownBusObjectCount, powerIsOn,
                 objServer, systemBus);

    io.run();
    return 0;
}
