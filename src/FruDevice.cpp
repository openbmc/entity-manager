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

#include <errno.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>

#include <Utils.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/container/flat_map.hpp>
#include <chrono>
#include <ctime>
#include <fstream>
#include <future>
#include <iostream>
#include <regex>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <thread>

extern "C" {
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
}

namespace fs = std::experimental::filesystem;
static constexpr bool DEBUG = false;
static size_t UNKNOWN_BUS_OBJECT_COUNT = 0;
constexpr size_t MAX_FRU_SIZE = 512;
constexpr size_t MAX_EEPROM_PAGE_INDEX = 255;
constexpr size_t busTimeoutSeconds = 5;

const static constexpr char *BASEBOARD_FRU_LOCATION =
    "/etc/fru/baseboard.fru.bin";

const static constexpr char *I2C_DEV_LOCATION = "/dev";

static constexpr std::array<const char *, 5> FRU_AREAS = {
    "INTERNAL", "CHASSIS", "BOARD", "PRODUCT", "MULTIRECORD"};
const static std::regex NON_ASCII_REGEX("[^\x01-\x7f]");
using DeviceMap = boost::container::flat_map<int, std::vector<char>>;
using BusMap = boost::container::flat_map<int, std::shared_ptr<DeviceMap>>;

struct FindDevicesWithCallback;

static bool isMuxBus(size_t bus)
{
    return is_symlink(std::experimental::filesystem::path(
        "/sys/bus/i2c/devices/i2c-" + std::to_string(bus) + "/mux_device"));
}

static int isDevice16Bit(int file)
{
    int byte1, byte2, i;

    /* Get first byte */
    if((byte1 = i2c_smbus_read_byte_data(file, 0)) < 0)
    {
        return byte1;
    }
    /* Read 7 more bytes, it will read same first byte in case of
     * 8 bit but it will read next byte in case of 16 bit
     */
		for (i = 0; i < 7; i++);
    {
        if((byte2 = i2c_smbus_read_byte_data(file, 0)) < 0)
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

static int read_block_data
    (int flag, int file, uint16_t offset, uint8_t len, uint8_t* buf)
{
    int ret;
    uint8_t low_addr, high_addr;

    low_addr = offset & 0xFF;
    high_addr = (offset >> 8) & 0xFF;

    if (flag == 0)
    {
        return i2c_smbus_read_i2c_block_data(file, low_addr, len, buf);
    }

    /* This is for 16 bit addressing EEPROM device. First an offset
     * needs to be written before read data from a offset
     */
    if ((ret = i2c_smbus_write_byte_data(file, 0, low_addr)) < 0)
    {
        return ret;
    }

    return i2c_smbus_read_i2c_block_data(file, high_addr, len, buf);
}

int get_bus_frus(int file, int first, int last, int bus,
                 std::shared_ptr<DeviceMap> devices)
{

    std::future<int> future = std::async(std::launch::async, [&]() {
        std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> block_data;
        int flag;

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
                std::cout << "something at bus " << bus << "addr " << ii
                          << "\n";
            }

            /* Check for Device type if it is 8 bit or 16 bit */
            if ((flag = isDevice16Bit(file)) < 0)
            {
                std::cerr << "failed to read bus " << bus << " address " << ii
                          << "\n";
                continue;
            }

            if (read_block_data(flag, file, 0x0, 0x8, block_data.data()) < 0)
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
                        if (read_block_data(
                                flag, file, area_offset, 0x8, block_data.data()) < 0)
                        {
                            std::cerr << "failed to read bus " << bus
                                      << " address " << ii << "\n";
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
                            if (read_block_data(
                                    flag, file, area_offset, to_get,
                                    block_data.data()) < 0)
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
                devices->emplace(ii, device);
            }
        }
        return 1;
    });
    std::future_status status =
        future.wait_for(std::chrono::seconds(busTimeoutSeconds));
    if (status == std::future_status::timeout)
    {
        std::cerr << "Error reading bus " << bus << "\n";
        return -1;
    }

    return future.get();
}

static void FindI2CDevices(const std::vector<fs::path> &i2cBuses,
                           std::shared_ptr<FindDevicesWithCallback> context,
                           boost::asio::io_service &io, BusMap &busMap)
{
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

        // don't scan muxed buses async as don't want to confuse the mux
        if (isMuxBus(bus))
        {
            get_bus_frus(file, 0x03, 0x77, bus, device);
            close(file);
        }
        else
        {
            io.post([file, device, bus, context] {
                //  i2cdetect by default uses the range 0x03 to 0x77, as
                //  this is
                //  what we
                //  have tested with, use this range. Could be changed in
                //  future.
                get_bus_frus(file, 0x03, 0x77, bus, device);
                close(file);
            });
        }
    }
}

// this class allows an async response after all i2c devices are discovered
struct FindDevicesWithCallback
    : std::enable_shared_from_this<FindDevicesWithCallback>
{
    FindDevicesWithCallback(const std::vector<fs::path> &i2cBuses,
                            boost::asio::io_service &io, BusMap &busMap,
                            std::function<void(void)> &&callback) :
        _i2cBuses(i2cBuses),
        _io(io), _busMap(busMap), _callback(std::move(callback))
    {
    }
    ~FindDevicesWithCallback()
    {
        _callback();
    }
    void run()
    {
        FindI2CDevices(_i2cBuses, shared_from_this(), _io, _busMap);
    }

    const std::vector<fs::path> &_i2cBuses;
    boost::asio::io_service &_io;
    BusMap &_busMap;
    std::function<void(void)> _callback;
};

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
        "PART_NUMBER", "SERIAL_NUMBER", "INFO_AM1", "INFO_AM2"};

    static const std::vector<const char *> BOARD_FRU_AREAS = {
        "MANUFACTURER",   "PRODUCT_NAME", "SERIAL_NUMBER", "PART_NUMBER",
        "FRU_VERSION_ID", "INFO_AM1",     "INFO_AM2"};

    static const std::vector<const char *> PRODUCT_FRU_AREAS = {
        "MANUFACTURER",   "PRODUCT_NAME",  "PART_NUMBER",
        "VERSION",        "SERIAL_NUMBER", "ASSET_TAG",
        "FRU_VERSION_ID", "INFO_AM1",      "INFO_AM2"};

    size_t sum = 0;

    if (fruBytes.size() <= 8)
    {
        return false;
    }
    std::vector<char>::const_iterator fruAreaOffsetField = fruBytes.begin();
    result["Common_Format_Version"] =
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

                /* Checking for length if field present */
                if (length == 0)
                {
                    result[std::string(area) + "_" + field] =
                        std::string("Null");
                    continue;
                }

                /* Checking for last byte C1 to indicate that no more
                 * field to be read */
                if (length == 1)
                {
                    break;
                }

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
    std::shared_ptr<sdbusplus::asio::connection> dbusConn,
    std::vector<char> &device, sdbusplus::asio::object_server &objServer,
    boost::container::flat_map<std::pair<size_t, size_t>,
                               std::shared_ptr<sdbusplus::asio::dbus_interface>>
        &dbusInterfaceMap,
    uint32_t bus, uint32_t address)
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

    productName = "/xyz/openbmc_project/FruDevice/" + productName;
    // avoid duplicates by checking to see if on a mux
    if (bus > 0)
    {
        size_t index = 0;
        for (auto const &busIface : dbusInterfaceMap)
        {
            if ((busIface.second->get_object_path() == productName))
            {
                if (isMuxBus(bus) && address == busIface.first.second)
                {
                    continue;
                }
                // add underscore _index for the same object path on dbus
                std::string strIndex = std::to_string(index);
                if (index > 0)
                {
                    productName.substr(0, productName.size() - strIndex.size());
                }
                else
                {
                    productName += "_";
                }
                productName += std::to_string(index++);
            }
        }
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface(productName, "xyz.openbmc_project.FruDevice");
    dbusInterfaceMap[std::pair<size_t, size_t>(bus, address)] = iface;

    for (auto &property : formattedFru)
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

bool writeFru(uint8_t bus, uint8_t address, const std::vector<uint8_t> &fru)
{
    boost::container::flat_map<std::string, std::string> tmp;
    if (fru.size() > MAX_FRU_SIZE)
    {
        std::cerr << "Invalid fru.size() during writeFru\n";
        return false;
    }
    // verify legal fru by running it through fru parsing logic
    if (!formatFru(reinterpret_cast<const std::vector<char> &>(fru), tmp))
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
        file.write(reinterpret_cast<const char *>(fru.data()), fru.size());
        return file.good();
    }
    else
    {
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

            if (i2c_smbus_write_byte_data(file, index & 0xFF, fru[index]) < 0)
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

void rescanBusses(
    boost::asio::io_service &io, BusMap &busMap,
    boost::container::flat_map<std::pair<size_t, size_t>,
                               std::shared_ptr<sdbusplus::asio::dbus_interface>>
        &dbusInterfaceMap,
    std::shared_ptr<sdbusplus::asio::connection> &systemBus,
    sdbusplus::asio::object_server &objServer)
{
    static boost::asio::deadline_timer timer(io);
    timer.expires_from_now(boost::posix_time::seconds(1));

    // setup an async wait in case we get flooded with requests
    timer.async_wait([&](const boost::system::error_code &ec) {
        auto devDir = fs::path("/dev/");
        auto matchString = std::string("i2c*");
        std::vector<fs::path> i2cBuses;

        if (!findFiles(devDir, matchString, i2cBuses))
        {
            std::cerr << "unable to find i2c devices\n";
            return;
        }
        // scanning muxes in reverse order seems to have adverse effects
        // sorting this list seems to be a fix for that
        std::sort(i2cBuses.begin(), i2cBuses.end());

        busMap.clear();
        auto scan = std::make_shared<FindDevicesWithCallback>(
            i2cBuses, io, busMap, [&]() {
                for (auto &busIface : dbusInterfaceMap)
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
                    busMap[0] = std::make_shared<DeviceMap>(baseboardDev);
                }
                for (auto &devicemap : busMap)
                {
                    for (auto &device : *devicemap.second)
                    {
                        AddFruObjectToDbus(systemBus, device.second, objServer,
                                           dbusInterfaceMap, devicemap.first,
                                           device.first);
                    }
                }
            });
        scan->run();
    });
}

int main(int argc, char **argv)
{
    auto devDir = fs::path("/dev/");
    auto matchString = std::string("i2c*");
    std::vector<fs::path> i2cBuses;

    if (!findFiles(devDir, matchString, i2cBuses))
    {
        std::cerr << "unable to find i2c devices\n";
        return 1;
    }

    boost::asio::io_service io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto objServer = sdbusplus::asio::object_server(systemBus);
    systemBus->request_name("com.intel.FruDevice");

    // this is a map with keys of pair(bus number, address) and values of
    // the object on dbus
    boost::container::flat_map<std::pair<size_t, size_t>,
                               std::shared_ptr<sdbusplus::asio::dbus_interface>>
        dbusInterfaceMap;
    BusMap busmap;

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface("/xyz/openbmc_project/FruDevice",
                                "xyz.openbmc_project.FruDeviceManager");

    iface->register_method("ReScan", [&]() {
        rescanBusses(io, busmap, dbusInterfaceMap, systemBus, objServer);
    });

    iface->register_method(
        "GetRawFru", [&](const uint8_t &bus, const uint8_t &address) {
            auto deviceMap = busmap.find(bus);
            if (deviceMap == busmap.end())
            {
                throw std::invalid_argument("Invalid Bus.");
            }
            auto device = deviceMap->second->find(address);
            if (device == deviceMap->second->end())
            {
                throw std::invalid_argument("Invalid Address.");
            }
            std::vector<uint8_t> &ret =
                reinterpret_cast<std::vector<uint8_t> &>(device->second);
            return ret;
        });

    iface->register_method("WriteFru", [&](const uint8_t bus,
                                           const uint8_t address,
                                           const std::vector<uint8_t> &data) {
        if (!writeFru(bus, address, data))
        {
            throw std::invalid_argument("Invalid Arguments.");
            return;
        }
        // schedule rescan on success
        rescanBusses(io, busmap, dbusInterfaceMap, systemBus, objServer);
    });
    iface->initialize();

    std::function<void(sdbusplus::message::message & message)> eventHandler =
        [&](sdbusplus::message::message &message) {
            std::string objectName;
            boost::container::flat_map<
                std::string, sdbusplus::message::variant<
                                 std::string, bool, int64_t, uint64_t, double>>
                values;
            message.read(objectName, values);
            auto findPgood = values.find("pgood");
            if (findPgood != values.end())
            {

                rescanBusses(io, busmap, dbusInterfaceMap, systemBus,
                             objServer);
            }
        };

    sdbusplus::bus::match::match powerMatch = sdbusplus::bus::match::match(
        static_cast<sdbusplus::bus::bus &>(*systemBus),
        "type='signal',interface='org.freedesktop.DBus.Properties',path_"
        "namespace='/xyz/openbmc_project/Chassis/Control/"
        "power0',arg0='xyz.openbmc_project.Chassis.Control.Power'",
        eventHandler);

    int fd = inotify_init();
    int wd = inotify_add_watch(fd, I2C_DEV_LOCATION,
                               IN_CREATE | IN_MOVED_TO | IN_DELETE);
    std::array<char, 4096> readBuffer;
    std::string pendingBuffer;
    // monitor for new i2c devices
    boost::asio::posix::stream_descriptor dirWatch(io, fd);
    std::function<void(const boost::system::error_code, std::size_t)>
        watchI2cBusses = [&](const boost::system::error_code &ec,
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
                const inotify_event *iEvent =
                    reinterpret_cast<const inotify_event *>(
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
                rescanBusses(io, busmap, dbusInterfaceMap, systemBus,
                             objServer);
            }

            dirWatch.async_read_some(boost::asio::buffer(readBuffer),
                                     watchI2cBusses);
        };

    dirWatch.async_read_some(boost::asio::buffer(readBuffer), watchI2cBusses);
    // run the initial scan
    rescanBusses(io, busmap, dbusInterfaceMap, systemBus, objServer);

    io.run();
    return 0;
}
