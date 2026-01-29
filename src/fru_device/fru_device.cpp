// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "../utils.hpp"
#include "fru_utils.hpp"

#include <fcntl.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <stdplus/fd/create.hpp>
#include <stdplus/fd/managed.hpp>
#include <stdplus/raw.hpp>

#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <flat_map>
#include <flat_set>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
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
constexpr size_t maxFruSize = 512;
constexpr size_t maxEepromPageIndex = 255;
constexpr size_t busTimeoutSeconds = 10;

constexpr const char* blocklistPath = PACKAGE_DIR "blacklist.json";

const static constexpr char* baseboardFruLocation =
    "/etc/fru/baseboard.fru.bin";

const static constexpr char* i2CDevLocation = "/dev";

constexpr const char* fruDevice16BitDetectMode = FRU_DEVICE_16BITDETECTMODE;

// TODO Refactor these to not be globals
// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
static std::flat_map<size_t, std::optional<std::flat_set<size_t>>> busBlocklist;
struct FindDevicesWithCallback;

static std::flat_map<std::pair<size_t, size_t>,
                     std::shared_ptr<sdbusplus::asio::dbus_interface>>
    foundDevices;

static std::flat_map<size_t, std::flat_set<size_t>> failedAddresses;
static std::flat_map<size_t, std::flat_set<size_t>> fruAddresses;

boost::asio::io_context io;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

bool updateFruProperty(
    const std::string& propertyValue, uint32_t bus, uint32_t address,
    const std::string& propertyName,
    std::flat_map<std::pair<size_t, size_t>,
                  std::shared_ptr<sdbusplus::asio::dbus_interface>>&
        dbusInterfaceMap,
    size_t& unknownBusObjectCount, const bool& powerIsOn,
    const std::set<size_t>& addressBlocklist,
    sdbusplus::asio::object_server& objServer);

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

static int64_t readFromEeprom(const std::shared_ptr<stdplus::ManagedFd>& fd,
                              off_t offset, std::span<uint8_t> outbuf)
{
    try
    {
        fd->lseek(offset, stdplus::fd::Whence::Set);
    }
    catch (const std::system_error& e)
    {
        lg2::error("failed to seek: {ERR}", "ERR", e.what());
        return -1;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    std::span<std::byte> byteBuf(reinterpret_cast<std::byte*>(outbuf.data()),
                                 outbuf.size());
    try
    {
        return fd->read(byteBuf).size();
    }
    catch (const std::system_error& e)
    {
        return -1;
    }
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
    bool fullMatch = false;
    fromCharsWrapper(num, val, fullMatch);
    return val;
}

static int getRootBus(size_t bus)
{
    auto ec = std::error_code();
    auto path = std::filesystem::read_symlink(
        std::filesystem::path(
            "/sys/bus/i2c/devices/i2c-" + std::to_string(bus) + "/mux_device"),
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

static bool isMuxBus(size_t bus)
{
    auto ec = std::error_code();
    auto isSymlink =
        is_symlink(std::filesystem::path("/sys/bus/i2c/devices/i2c-" +
                                         std::to_string(bus) + "/mux_device"),
                   ec);
    return (!ec && isSymlink);
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

// Issue an I2C transaction to first write to_target_buf_len bytes,then read
// from_target_buf_len bytes.
static int i2cSmbusWriteThenRead(
    const std::shared_ptr<stdplus::ManagedFd>& fd, uint16_t address,
    std::span<uint8_t> toTargetBuf, std::span<uint8_t> fromTargetBuf)
{
    if (toTargetBuf.empty() || fromTargetBuf.empty())
    {
        return -1;
    }

    constexpr size_t smbusWriteThenReadMsgCount = 2;
    std::array<struct i2c_msg, smbusWriteThenReadMsgCount> msgs{};
    struct i2c_rdwr_ioctl_data rdwr{};

    msgs[0].addr = address;
    msgs[0].flags = 0;
    msgs[0].len = toTargetBuf.size();
    msgs[0].buf = const_cast<uint8_t*>(toTargetBuf.data());
    msgs[1].addr = address;
    msgs[1].flags = I2C_M_RD;
    msgs[1].len = fromTargetBuf.size();
    msgs[1].buf = const_cast<uint8_t*>(fromTargetBuf.data());

    rdwr.msgs = msgs.data();
    rdwr.nmsgs = msgs.size();

    int ret = 0;
    try
    {
        ret = fd->ioctl(I2C_RDWR, &rdwr);
    }
    catch (const std::system_error& e)
    {
        lg2::error("failed call i2cSmbusWriteThenRead: {ERR}", "ERR", e.what());
        return -1;
    }
    return (ret == static_cast<int>(msgs.size())) ? msgs[1].len : -1;
}

static int64_t readData(bool is16bit, bool isBytewise,
                        const std::shared_ptr<stdplus::ManagedFd>& fd,
                        uint16_t address, off_t offset, std::span<uint8_t> buf)
{
    if (!is16bit)
    {
        if (!isBytewise)
        {
            return i2c_smbus_read_i2c_block_data(
                fd->get(), static_cast<uint8_t>(offset), buf.size(),
                const_cast<uint8_t*>(buf.data()));
        }

        for (size_t i = 0; i < buf.size(); i++)
        {
            int byte = i2c_smbus_read_byte_data(
                fd->get(), static_cast<uint8_t>(offset + i));
            if (byte < 0)
            {
                return static_cast<int64_t>(byte);
            }
            buf[i] = static_cast<uint8_t>(byte);
        }
        return static_cast<int64_t>(buf.size());
    }

    offset = htobe16(offset);
    return i2cSmbusWriteThenRead(fd, address,
                                 stdplus::raw::asSpan<uint8_t>(offset), buf);
}

// Mode_1:
// --------
// Please refer to document docs/address_size_detection_modes.md for
// more details and explanations.
static std::optional<bool> isDevice16BitMode1(
    const std::shared_ptr<stdplus::ManagedFd>& fd)
{
    // Set the higher data word address bits to 0. It's safe on 8-bit
    // addressing EEPROMs because it doesn't write any actual data.
    int ret = i2c_smbus_write_byte(fd->get(), 0);
    if (ret < 0)
    {
        return std::nullopt;
    }

    /* Get first byte */
    int byte1 = i2c_smbus_read_byte_data(fd->get(), 0);
    if (byte1 < 0)
    {
        return std::nullopt;
    }
    /* Read 7 more bytes, it will read same first byte in case of
     * 8 bit but it will read next byte in case of 16 bit
     */
    for (int i = 0; i < 7; i++)
    {
        int byte2 = i2c_smbus_read_byte_data(fd->get(), 0);
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

// Mode_2:
// --------
// Please refer to document docs/address_size_detection_modes.md for
// more details and explanations.
static std::optional<bool> isDevice16BitMode2(
    const std::shared_ptr<stdplus::ManagedFd>& fd, uint16_t address)
{
    uint8_t first = 0;
    uint8_t cur = 0;
    uint16_t v = 0;
    int ret = 0;

    /*
     * Write 2 bytes byte0 = 0, byte1 = {0..7} and then subsequent read byte
     * It will read same first byte in case of 8 bit but
     * it will read next byte in case of 16 bit
     */
    for (int i = 0; i < 8; i++)
    {
        v = htobe16(i);

        ret =
            i2cSmbusWriteThenRead(fd, address, stdplus::raw::asSpan<uint8_t>(v),
                                  stdplus::raw::asSpan<uint8_t>(cur));
        if (ret < 0)
        {
            return std::nullopt;
        }

        if (i == 0)
        {
            first = cur;
        }

        if (first != cur)
        {
            return true;
        }
    }
    return false;
}

static std::optional<bool> isDevice16Bit(
    const std::shared_ptr<stdplus::ManagedFd>& fd, uint16_t address)
{
    std::string mode(fruDevice16BitDetectMode);

    if (mode == "MODE_2")
    {
        return isDevice16BitMode2(fd, address);
    }

    return isDevice16BitMode1(fd);
}

// TODO: This code is very similar to the non-eeprom version and can be merged
// with some tweaks.
static std::vector<uint8_t> processEeprom(int bus, int address)
{
    auto path = getEepromPath(bus, address);
    std::shared_ptr<stdplus::ManagedFd> fd;
    try
    {
        fd = std::make_shared<stdplus::ManagedFd>(stdplus::fd::open(
            path.c_str(),
            stdplus::fd::OpenFlags(stdplus::fd::OpenAccess::ReadOnly)));
    }
    catch (const std::system_error& e)
    {
        lg2::error("Unable to open eeprom file: {PATH}: {ERR}", "PATH", path,
                   "ERR", e.what());
        return {};
    }

    std::string errorMessage = "eeprom at " + std::to_string(bus) +
                               " address " + std::to_string(address);
    auto readFunc = [fd](off_t offset, std::span<uint8_t> outbuf) {
        return readFromEeprom(fd, offset, outbuf);
    };
    FRUReader reader(std::move(readFunc));
    std::pair<std::vector<uint8_t>, bool> pair =
        readFRUContents(reader, errorMessage);

    return pair.first;
}

std::set<size_t> findI2CEeproms(int i2cBus,
                                const std::shared_ptr<DeviceMap>& devices)
{
    std::set<size_t> foundList;

    std::string path = "/sys/bus/i2c/devices/i2c-" + std::to_string(i2cBus);

    // For each file listed under the i2c device
    // NOTE: This should be faster than just checking for each possible address
    // path.
    auto ec = std::error_code();
    for (const auto& p : fs::directory_iterator(path, ec))
    {
        if (ec)
        {
            lg2::error("directory_iterator err {ERR}", "ERR", ec.message());
            break;
        }
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
            lg2::error("regex didn't capture");
            continue;
        }

        std::ssub_match subMatch = m[1];
        std::string addressString = subMatch.str();
        std::string_view addressStringView(addressString);

        size_t address = 0;
        std::from_chars(addressStringView.begin(), addressStringView.end(),
                        address, 16);

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

bool getBusFRUs(std::shared_ptr<stdplus::ManagedFd> fd, int first, int last,
                int bus, std::shared_ptr<DeviceMap> devices,
                const bool& powerIsOn, const std::set<size_t>& addressBlocklist,
                sdbusplus::asio::object_server& objServer)
{
    std::weak_ptr<stdplus::ManagedFd> weakFd = fd;
    auto promise = std::make_shared<std::promise<bool>>();
    std::future<bool> future = promise->get_future();
    std::thread t([weakFd, bus, first, last, devices, &powerIsOn,
                   &addressBlocklist, &objServer, promise]() {
        auto runner = [&]() {
            // NOTE: When reading the devices raw on the bus, it can interfere
            // with the driver's ability to operate, therefore read eeproms
            // first before scanning for devices without drivers. Several
            // experiments were run and it was determined that if there were any
            // devices on the bus before the eeprom was hit and read, the eeprom
            // driver wouldn't open while the bus device was open. An experiment
            // was not performed to see if this issue was resolved if the i2c
            // bus device was closed, but hexdumps of the eeprom later were
            // successful.

            // Scan for i2c eeproms loaded on this bus.
            std::set<size_t> skipList = findI2CEeproms(bus, devices);
            std::flat_set<size_t>& failedItems = failedAddresses[bus];
            std::flat_set<size_t>& foundItems = fruAddresses[bus];
            foundItems.clear();

            skipList.insert_range(addressBlocklist);

            auto busFind = busBlocklist.find(bus);
            if (busFind != busBlocklist.end())
            {
                if (busFind->second != std::nullopt)
                {
                    for (const auto& address : *(busFind->second))
                    {
                        skipList.insert(address);
                    }
                }
            }

            std::flat_set<size_t>* rootFailures = nullptr;
            int rootBus = getRootBus(bus);

            if (rootBus >= 0)
            {
                auto rootBusFind = busBlocklist.find(rootBus);
                if (rootBusFind != busBlocklist.end())
                {
                    if (rootBusFind->second != std::nullopt)
                    {
                        for (const auto& rootAddress : *(rootBusFind->second))
                        {
                            skipList.insert(rootAddress);
                        }
                    }
                }
                rootFailures = &(failedAddresses[rootBus]);
                foundItems = fruAddresses[rootBus];
            }

            constexpr int startSkipTargetAddr = 0;
            constexpr int endSkipTargetAddr = 12;

            for (int ii = first; ii <= last; ii++)
            {
                if (foundItems.contains(ii))
                {
                    continue;
                }
                if (skipList.contains(ii))
                {
                    continue;
                }
                // skipping since no device is present in this range
                if (ii >= startSkipTargetAddr && ii <= endSkipTargetAddr)
                {
                    continue;
                }

                std::shared_ptr<stdplus::ManagedFd> newFd = weakFd.lock();
                if (newFd == nullptr)
                {
                    lg2::error(
                        "File descriptor at bus {BUS} is closed due to timeout",
                        "BUS", bus);
                    return false;
                }
                // Set target address
                try
                {
                    if (ioctl(newFd->get(), I2C_SLAVE, ii) < 0)
                    {
                        lg2::error("device at bus {BUS} address {ADDR} busy",
                                   "BUS", bus, "ADDR", ii);
                        continue;
                    }
                }
                catch (const std::system_error& e)
                {
                    lg2::error("device at bus {BUS} address {ADDR} busy: {ERR}",
                               "BUS", bus, "ADDR", ii, "ERR", e.what());
                    continue;
                }
                // probe
                if (i2c_smbus_read_byte(newFd->get()) < 0)
                {
                    continue;
                }

                lg2::debug("something at bus {BUS}, addr {ADDR}", "BUS", bus,
                           "ADDR", ii);

                makeProbeInterface(bus, ii, objServer);

                if (failedItems.contains(ii))
                {
                    // if we failed to read it once, unlikely we can read it
                    // later
                    continue;
                }

                if (rootFailures != nullptr)
                {
                    if (rootFailures->contains(ii))
                    {
                        continue;
                    }
                }

                /* Check for Device type if it is 8 bit or 16 bit */
                std::optional<bool> is16Bit = isDevice16Bit(newFd, ii);
                if (!is16Bit.has_value())
                {
                    lg2::error("failed to read bus {BUS} address {ADDR}", "BUS",
                               bus, "ADDR", ii);
                    if (powerIsOn)
                    {
                        failedItems.insert(ii);
                    }
                    continue;
                }
                bool is16BitBool{*is16Bit};

                auto readFunc = [is16BitBool, newFd,
                                 ii](off_t offset, std::span<uint8_t> outbuf) {
                    return readData(is16BitBool, false, newFd, ii, offset,
                                    outbuf);
                };
                FRUReader reader(std::move(readFunc));
                std::string errorMessage = "bus " + std::to_string(bus) +
                                           " address " + std::to_string(ii);
                std::pair<std::vector<uint8_t>, bool> pair =
                    readFRUContents(reader, errorMessage);
                const bool foundHeader = pair.second;

                if (!foundHeader && !is16BitBool)
                {
                    // certain FRU eeproms require bytewise reading.
                    // otherwise garbage is read. e.g. SuperMicro PWS 920P-SQ

                    auto readFunc =
                        [is16BitBool, newFd,
                         ii](off_t offset, std::span<uint8_t> outbuf) {
                            return readData(is16BitBool, true, newFd, ii,
                                            offset, outbuf);
                        };
                    FRUReader readerBytewise(std::move(readFunc));
                    pair = readFRUContents(readerBytewise, errorMessage);
                }

                if (pair.first.empty())
                {
                    continue;
                }

                devices->emplace(ii, pair.first);
                fruAddresses[bus].insert(ii);
            }
            return true;
        };

        try
        {
            promise->set_value(runner());
        }
        catch (...)
        {
            promise->set_exception(std::current_exception());
        }
    });
    std::future_status status =
        future.wait_for(std::chrono::seconds(busTimeoutSeconds));
    if (status == std::future_status::timeout)
    {
        lg2::error("Error reading bus {BUS}", "BUS", bus);
        t.detach();
        if (powerIsOn)
        {
            busBlocklist[bus] = std::nullopt;
        }
        return false;
    }

    t.join();
    return future.get();
}

struct AddressBlocklistResult
{
    int rc;
    std::set<size_t> list;
};

AddressBlocklistResult loadAddressBlocklist(const nlohmann::json& data)
{
    auto addrIt = data.find("addresses");
    if (addrIt == data.end())
    {
        return {0, std::set<size_t>()};
    }

    const auto* const addr =
        data["addresses"].get_ptr<const nlohmann::json::array_t*>();

    if (addr == nullptr)
    {
        lg2::error("addresses must be an array");
        return {EINVAL, std::set<size_t>()};
    }

    std::set<size_t> addressBlocklist = {};

    for (const auto& address : *addr)
    {
        const auto* addrS = address.get_ptr<const std::string*>();
        if (addrS == nullptr)
        {
            lg2::error("address must be a string\n");
            return {EINVAL, std::set<size_t>()};
        }

        if (!(addrS->starts_with("0x") || addrS->starts_with("0X")))
        {
            lg2::error("address must start with 0x or 0X\n");
            return {EINVAL, std::set<size_t>()};
        }

        // The alternative offered here relies on undefined behavior
        // of dereferencing iterators given by .end()
        // this pointer access is checked above by the calls to starts_with
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        size_t addressInt = 0;
        auto [ptr, ec] = std::from_chars(
            addrS->data() + 2, addrS->data() + addrS->length(), addressInt, 16);

        const auto erc = std::make_error_condition(ec);
        if (ptr != (addrS->data() + addrS->length()) || erc)
        {
            lg2::error("Invalid address type: {ADDR} {MSG}\n", "ADDR", *addrS,
                       "MSG", erc.message());
            return {EINVAL, std::set<size_t>()};
        }
        // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (addressInt > 0x77)
        {
            lg2::error("Invalid address {ADDR}\n", "ADDR", *addrS);
            return {EINVAL, std::set<size_t>()};
        }

        addressBlocklist.insert(addressInt);
    }

    return {0, addressBlocklist};
}

// once the bus blocklist is made non global,
// return it here too.
std::set<size_t> loadBlocklist(const char* path)
{
    std::ifstream blocklistStream(path);
    if (!blocklistStream.good())
    {
        // File is optional.
        lg2::error("Cannot open blocklist file.\n");
        return {};
    }

    nlohmann::json data =
        nlohmann::json::parse(blocklistStream, nullptr, false);
    if (data.is_discarded())
    {
        lg2::error(
            "Illegal blocklist file detected, cannot validate JSON, exiting");
        std::exit(EXIT_FAILURE);
    }

    // It's expected to have at least one field, "buses" that is an array of the
    // buses by integer. Allow for future options to exclude further aspects,
    // such as specific addresses or ranges.
    if (data.type() != nlohmann::json::value_t::object)
    {
        lg2::error("Illegal blocklist, expected to read dictionary");
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
            lg2::error("Invalid contents for blocklist buses field");
            std::exit(EXIT_FAILURE);
        }

        // Catch exception here for type mis-match.
        try
        {
            for (const auto& busIterator : buses)
            {
                // If bus and addresses field are missing, that's fine.
                if (busIterator.contains("bus") &&
                    busIterator.contains("addresses"))
                {
                    auto busData = busIterator.at("bus");
                    auto bus = busData.get<size_t>();

                    auto addressData = busIterator.at("addresses");
                    auto addresses =
                        addressData.get<std::set<std::string_view>>();

                    auto& block = busBlocklist[bus].emplace();
                    for (const auto& address : addresses)
                    {
                        size_t addressInt = 0;
                        bool fullMatch = false;
                        fromCharsWrapper(address.substr(2), addressInt,
                                         fullMatch, 16);
                        block.insert(addressInt);
                    }
                }
                else
                {
                    busBlocklist[busIterator.get<size_t>()] = std::nullopt;
                }
            }
        }
        catch (const nlohmann::detail::type_error& e)
        {
            // Type mis-match is a critical error.
            lg2::error("Invalid bus type: {ERR}", "ERR", e.what());
            std::exit(EXIT_FAILURE);
        }
    }

    const auto [rc, addressBlocklist] = loadAddressBlocklist(data);
    if (rc != 0)
    {
        std::exit(EXIT_FAILURE);
    }

    return addressBlocklist;
}

static std::vector<fs::path> findI2CDevices(
    const std::vector<fs::path>& i2cBuses, BusMap& busmap,
    const bool& powerIsOn, const std::set<size_t>& addressBlocklist,
    sdbusplus::asio::object_server& objServer)
{
    std::vector<fs::path> retryI2cBuses;
    for (const auto& i2cBus : i2cBuses)
    {
        int bus = busStrToInt(i2cBus.string());

        if (bus < 0)
        {
            lg2::error("Cannot translate {BUS} to int", "BUS", i2cBus);
            continue;
        }
        auto busFind = busBlocklist.find(bus);
        if (busFind != busBlocklist.end())
        {
            if (busFind->second == std::nullopt)
            {
                continue; // Skip blocked busses.
            }
        }
        int rootBus = getRootBus(bus);
        auto rootBusFind = busBlocklist.find(rootBus);
        if (rootBusFind != busBlocklist.end())
        {
            if (rootBusFind->second == std::nullopt)
            {
                continue;
            }
        }

        std::shared_ptr<stdplus::ManagedFd> fd;
        try
        {
            fd = std::make_shared<stdplus::ManagedFd>(stdplus::fd::open(
                i2cBus.c_str(), stdplus::fd::OpenAccess::ReadWrite));
        }
        catch (const std::system_error& e)
        {
            lg2::error("unable to open i2c device {PATH}: {ERR}", "PATH",
                       i2cBus.string(), "ERR", e.what());
            continue;
        }
        unsigned long funcs = 0;

        try
        {
            if (ioctl(fd->get(), I2C_FUNCS, &funcs) < 0)
            {
                lg2::error(
                    "Error: Could not get the adapter functionality matrix bus {BUS}",
                    "BUS", bus);
                continue;
            }
        }
        catch (const std::system_error& e)
        {
            lg2::error(
                "Error: Could not get the adapter functionality matrix bus {BUS}: {ERR}",
                "BUS", bus, "ERR", e.what());
            continue;
        }
        if (((funcs & I2C_FUNC_SMBUS_READ_BYTE) == 0U) ||
            ((funcs & I2C_FUNC_SMBUS_READ_I2C_BLOCK) == 0U))
        {
            lg2::error("Error: Can't use SMBus Receive Byte command bus {BUS}",
                       "BUS", bus);
            continue;
        }
        auto& device = busmap[bus];
        device = std::make_shared<DeviceMap>();

        //  i2cdetect by default uses the range 0x03 to 0x77, as
        //  this is  what we have tested with, use this range. Could be
        //  changed in future.
        lg2::debug("Scanning bus {BUS}", "BUS", bus);

        // fd is closed in this function in case the bus locks up
        // If the bus is locked up, we will retry for up to {maxRetries} times.
        if (!getBusFRUs(fd, 0x03, 0x77, bus, device, powerIsOn,
                        addressBlocklist, objServer))
        {
            retryI2cBuses.push_back(i2cBus);
        }

        lg2::debug("Done scanning bus {BUS}", "BUS", bus);
    }

    return retryI2cBuses;
}

// this class allows an async response after all i2c devices are discovered
struct FindDevicesWithCallback :
    std::enable_shared_from_this<FindDevicesWithCallback>
{
    FindDevicesWithCallback(
        const std::vector<fs::path>& i2cBuses, BusMap& busmap,
        const bool& powerIsOn, sdbusplus::asio::object_server& objServer,
        const std::set<size_t>& addressBlocklist,
        std::function<void()>&& callback, size_t retries = maxRetries) :
        _i2cBuses(i2cBuses), _busMap(busmap), _powerIsOn(powerIsOn),
        _objServer(objServer), _callback(std::move(callback)),
        _addressBlocklist{addressBlocklist}, _retries(retries)
    {}
    ~FindDevicesWithCallback()
    {
        _callback();
        if (_retryI2cBuses.empty())
        {
            lg2::info("All I2C devices discovered successfully.");
            return;
        }
        if (maxRetries == 0)
        {
            return;
        }
        if (_retries == 0)
        {
            lg2::error(
                "Failed to discover all I2C devices after {MAX_RETRIES} retries.",
                "MAX_RETRIES", maxRetries);
            return;
        }

        auto scan = std::make_shared<FindDevicesWithCallback>(
            _i2cBuses, _busMap, _powerIsOn, _objServer, _addressBlocklist,
            std::move(_callback), _retries - 1);
        auto timer = std::make_shared<boost::asio::steady_timer>(io);
        timer->expires_after(retryDelay);
        timer->async_wait([timer, scan](const boost::system::error_code&) {
            scan->run();
        });
    }
    void run()
    {
        _retryI2cBuses = findI2CDevices(_i2cBuses, _busMap, _powerIsOn,
                                        _addressBlocklist, _objServer);
    }

    const std::vector<fs::path>& _i2cBuses;
    BusMap& _busMap;
    const bool& _powerIsOn;
    sdbusplus::asio::object_server& _objServer;
    std::function<void()> _callback;
    std::set<size_t> _addressBlocklist;
    size_t _retries;
    std::vector<fs::path> _retryI2cBuses{};

    static constexpr size_t maxRetries = FRU_DEVICE_MAXRETRIES;
    static constexpr std::chrono::seconds retryDelay{FRU_DEVICE_RETRYDELAY};
};

void addFruObjectToDbus(
    std::vector<uint8_t>& device,
    std::flat_map<std::pair<size_t, size_t>,
                  std::shared_ptr<sdbusplus::asio::dbus_interface>>&
        dbusInterfaceMap,
    uint32_t bus, uint32_t address, size_t& unknownBusObjectCount,
    const bool& powerIsOn, const std::set<size_t>& addressBlocklist,
    sdbusplus::asio::object_server& objServer)
{
    std::flat_map<std::string, std::string, std::less<>> formattedFRU;

    std::optional<std::string> optionalProductName = getProductName(
        device, formattedFRU, bus, address, unknownBusObjectCount);
    if (!optionalProductName)
    {
        lg2::error("getProductName failed. product name is empty.");
        return;
    }

    std::string productName =
        "/xyz/openbmc_project/FruDevice/" + optionalProductName.value();

    std::optional<int> index = findIndexForFRU(dbusInterfaceMap, productName);
    if (index.has_value())
    {
        productName += "_";
        productName += std::to_string(++(*index));
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface(productName, "xyz.openbmc_project.FruDevice");
    dbusInterfaceMap[std::pair<size_t, size_t>(bus, address)] = iface;

    if (ENABLE_FRU_UPDATE_PROPERTY)
    {
        iface->register_method(
            "UpdateFruField",
            [bus, address, &dbusInterfaceMap, &unknownBusObjectCount,
             &powerIsOn, &objServer, addressBlocklist](
                const std::string& fieldName, const std::string& fieldValue) {
                // Update the property
                if (!updateFruProperty(fieldValue, bus, address, fieldName,
                                       dbusInterfaceMap, unknownBusObjectCount,
                                       powerIsOn, addressBlocklist, objServer))
                {
                    lg2::debug(
                        "Failed to Add Field: Name = {NAME}, Value = {VALUE}",
                        "NAME", fieldName, "VALUE", fieldValue);
                    return false;
                }

                return true;
            });
    }

    for (auto property : formattedFRU)
    {
        std::regex_replace(property.second.begin(), property.second.begin(),
                           property.second.end(), nonAsciiRegex, "_");
        if (property.second.empty())
        {
            continue;
        }
        std::string key =
            std::regex_replace(property.first, nonAsciiRegex, "_");

        // Allow FRU field update if ENABLE_FRU_UPDATE_PROPERTY is set.
        if (isFieldEditable(property.first))
        {
            std::string propertyName = property.first;
            iface->register_property(
                key, property.second + '\0',
                [bus, address, propertyName, &dbusInterfaceMap,
                 &unknownBusObjectCount, &powerIsOn, &objServer,
                 &addressBlocklist](const std::string& req, std::string& resp) {
                    if (strcmp(req.c_str(), resp.c_str()) != 0)
                    {
                        // call the method which will update
                        if (updateFruProperty(req, bus, address, propertyName,
                                              dbusInterfaceMap,
                                              unknownBusObjectCount, powerIsOn,
                                              addressBlocklist, objServer))
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
            lg2::error("illegal key: {KEY}", "KEY", key);
        }
        lg2::debug("parsed FRU property: {FIRST}: {SECOND}", "FIRST",
                   property.first, "SECOND", property.second);
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

bool writeFRU(uint8_t bus, uint8_t address, std::span<const uint8_t> fru)
{
    std::flat_map<std::string, std::string, std::less<>> tmp;
    if (fru.size() > maxFruSize)
    {
        lg2::error("Invalid fru.size() during writeFRU");
        return false;
    }
    // verify legal fru by running it through fru parsing logic
    if (formatIPMIFRU(fru, tmp) != resCodes::resOK)
    {
        lg2::error("Invalid fru format during writeFRU");
        return false;
    }
    // baseboard fru
    if (bus == 0 && address == 0)
    {
        std::ofstream file(baseboardFruLocation, std::ios_base::binary);
        if (!file.good())
        {
            lg2::error("Error opening file {PATH}", "PATH",
                       baseboardFruLocation);
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
        off_t offset = 0;
        std::shared_ptr<stdplus::ManagedFd> eeprom;
        try
        {
            eeprom = std::make_shared<stdplus::ManagedFd>(stdplus::fd::open(
                path.c_str(),
                stdplus::BitFlags<stdplus::fd::OpenFlag>(
                    static_cast<int>(stdplus::fd::OpenAccess::ReadWrite) |
                    static_cast<int>(stdplus::fd::OpenFlag::CloseOnExec))));
        }
        catch (const std::system_error& e)
        {
            lg2::error("unable to open i2c device {PATH}: {ERR}", "PATH", path,
                       "ERR", e.what());
            throw DBusInternalError();
            return false;
        }

        std::string errorMessage = "eeprom at " + std::to_string(bus) +
                                   " address " + std::to_string(address);
        auto readFunc = [eeprom](off_t offset, std::span<uint8_t> outbuf) {
            return readFromEeprom(eeprom, offset, outbuf);
        };
        FRUReader reader(std::move(readFunc));

        auto sections = findFRUHeader(reader, errorMessage, 0);
        if (!sections)
        {
            offset = 0;
        }
        else
        {
            offset = sections->IpmiFruOffset;
        }

        try
        {
            eeprom->lseek(offset, stdplus::fd::Whence::Set);
        }
        catch (const std::system_error& e)
        {
            lg2::error(
                "Unable to seek to offset {OFFSET} in device: {PATH}: {ERR}",
                "OFFSET", offset, "PATH", path, "ERR", e.what());
            throw DBusInternalError();
        }

        std::span<const std::byte> fruSpan(
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            reinterpret_cast<const std::byte*>(fru.data()), fru.size());
        try
        {
            eeprom->write(fruSpan);
        }
        catch (const std::system_error& e)
        {
            lg2::error("unable to write to i2zc device {PATH}: {ERR}", "PATH",
                       path, "ERR", e.what());
            throw DBusInternalError();
            return false;
        }
        return true;
    }

    std::string i2cBus = "/dev/i2c-" + std::to_string(bus);
    std::shared_ptr<stdplus::ManagedFd> fd;
    try
    {
        fd = std::make_shared<stdplus::ManagedFd>(stdplus::fd::open(
            i2cBus.c_str(),
            stdplus::BitFlags<stdplus::fd::OpenFlag>(
                static_cast<int>(stdplus::fd::OpenAccess::ReadWrite) |
                static_cast<int>(stdplus::fd::OpenFlag::CloseOnExec))));
    }
    catch (const std::system_error& e)
    {
        lg2::error("unable to open i2c device {PATH}: {ERR}", "PATH", i2cBus,
                   "ERR", e.what());
        throw DBusInternalError();
        return false;
    }
    try
    {
        if (ioctl(fd->get(), I2C_SLAVE_FORCE, address) < 0)
        {
            lg2::error("unable to set device address");
            throw DBusInternalError();
            return false;
        }
    }
    catch (const std::system_error& e)
    {
        lg2::error("unable to set device address: {ERR}", "ERR", e.what());
        throw e;
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
            try
            {
                if (ioctl(fd->get(), I2C_SLAVE_FORCE, ++address) < 0)
                {
                    lg2::error("unable to set device address");
                    throw DBusInternalError();
                    return false;
                }
            }
            catch (const std::system_error& e)
            {
                lg2::error("unable to set device address: {ERR}", "ERR",
                           e.what());
                throw e;
            }
        }

        if (i2c_smbus_write_byte_data(fd->get(), static_cast<uint8_t>(index),
                                      fru[index]) < 0)
        {
            if ((retries--) == 0U)
            {
                lg2::error("error writing fru: {ERR}", "ERR", strerror(errno));
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
    return true;
}

void rescanOneBus(
    BusMap& busmap, uint16_t busNum,
    std::flat_map<std::pair<size_t, size_t>,
                  std::shared_ptr<sdbusplus::asio::dbus_interface>>&
        dbusInterfaceMap,
    bool dbusCall, size_t& unknownBusObjectCount, const bool& powerIsOn,
    const std::set<size_t>& addressBlocklist,
    sdbusplus::asio::object_server& objServer)
{
    for (auto device = foundDevices.begin(); device != foundDevices.end();)
    {
        if (device->first.first == static_cast<size_t>(busNum))
        {
            objServer.remove_interface(device->second);
            device = foundDevices.erase(device);
        }
        else
        {
            device++;
        }
    }

    fs::path busPath = fs::path("/dev/i2c-" + std::to_string(busNum));
    if (!fs::exists(busPath))
    {
        if (dbusCall)
        {
            lg2::error("Unable to access i2c bus {BUS}", "BUS",
                       static_cast<int>(busNum));
            throw std::invalid_argument("Invalid Bus.");
        }
        return;
    }

    std::vector<fs::path> i2cBuses;
    i2cBuses.emplace_back(busPath);

    auto scan = std::make_shared<FindDevicesWithCallback>(
        i2cBuses, busmap, powerIsOn, objServer, addressBlocklist,
        [busNum, &busmap, &dbusInterfaceMap, &unknownBusObjectCount, &powerIsOn,
         &objServer, &addressBlocklist]() {
            for (auto busIface = dbusInterfaceMap.begin();
                 busIface != dbusInterfaceMap.end();)
            {
                if (busIface->first.first == static_cast<size_t>(busNum))
                {
                    objServer.remove_interface(busIface->second);
                    busIface = dbusInterfaceMap.erase(busIface);
                }
                else
                {
                    busIface++;
                }
            }
            auto found = busmap.find(busNum);
            if (found == busmap.end() || found->second == nullptr)
            {
                return;
            }
            for (auto device : *(found->second))
            {
                addFruObjectToDbus(device.second, dbusInterfaceMap,
                                   static_cast<uint32_t>(busNum), device.first,
                                   unknownBusObjectCount, powerIsOn,
                                   addressBlocklist, objServer);
            }
        });
    scan->run();
}

void rescanBusses(
    BusMap& busmap,
    std::flat_map<std::pair<size_t, size_t>,
                  std::shared_ptr<sdbusplus::asio::dbus_interface>>&
        dbusInterfaceMap,
    size_t& unknownBusObjectCount, const bool& powerIsOn,
    const std::set<size_t>& addressBlocklist,
    sdbusplus::asio::object_server& objServer)
{
    static boost::asio::steady_timer timer(io);
    timer.expires_after(std::chrono::seconds(1));

    // setup an async wait in case we get flooded with requests
    timer.async_wait([&](const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted)
        {
            return;
        }

        if (ec)
        {
            lg2::error("Error in timer: {ERR}", "ERR", ec.message());
            return;
        }

        auto devDir = fs::path("/dev/");
        std::vector<fs::path> i2cBuses;

        std::flat_map<size_t, fs::path> busPaths;
        if (!getI2cDevicePaths(devDir, busPaths))
        {
            lg2::error("unable to find i2c devices");
            return;
        }

        for (const auto& busPath : busPaths)
        {
            i2cBuses.emplace_back(busPath.second);
        }

        busmap.clear();
        for (auto [pair, interface] : foundDevices)
        {
            objServer.remove_interface(interface);
        }
        foundDevices.clear();

        auto scan = std::make_shared<FindDevicesWithCallback>(
            i2cBuses, busmap, powerIsOn, objServer, addressBlocklist, [&]() {
                for (auto busIface : dbusInterfaceMap)
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
                for (auto devicemap : busmap)
                {
                    for (auto device : *devicemap.second)
                    {
                        addFruObjectToDbus(device.second, dbusInterfaceMap,
                                           devicemap.first, device.first,
                                           unknownBusObjectCount, powerIsOn,
                                           addressBlocklist, objServer);
                    }
                }
            });
        scan->run();
    });
}

bool updateFruProperty(
    const std::string& propertyValue, uint32_t bus, uint32_t address,
    const std::string& propertyName,
    std::flat_map<std::pair<size_t, size_t>,
                  std::shared_ptr<sdbusplus::asio::dbus_interface>>&
        dbusInterfaceMap,
    size_t& unknownBusObjectCount, const bool& powerIsOn,
    const std::set<size_t>& addressBlocklist,
    sdbusplus::asio::object_server& objServer)
{
    lg2::debug(
        "updateFruProperty called: FieldName = {NAME}, FieldValue = {VALUE}",
        "NAME", propertyName, "VALUE", propertyValue);

    std::vector<uint8_t> fruData;
    if (!getFruData(fruData, bus, address))
    {
        lg2::error("Failure getting FRU Data from bus {BUS}, address {ADDRESS}",
                   "BUS", bus, "ADDRESS", address);
        return false;
    }

    bool success = updateAddProperty(propertyValue, propertyName, fruData);
    if (!success)
    {
        lg2::error(
            "Failed to update the property on bus {BUS}, address {ADDRESS}",
            "BUS", bus, "ADDRESS", address);
        return false;
    }

    if (!writeFRU(static_cast<uint8_t>(bus), static_cast<uint8_t>(address),
                  fruData))
    {
        lg2::error("Failed to write the FRU");
        return false;
    }

    rescanBusses(busMap, dbusInterfaceMap, unknownBusObjectCount, powerIsOn,
                 addressBlocklist, objServer);
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
        lg2::error("unable to find i2c devices");
        return 1;
    }

    // check for and load blocklist with initial buses.
    // once busBlocklist is moved to be non global,
    // add it here
    auto addressBlocklist = loadBlocklist(blocklistPath);

    systemBus->request_name("xyz.openbmc_project.FruDevice");

    // this is a map with keys of pair(bus number, address) and values of
    // the object on dbus
    std::flat_map<std::pair<size_t, size_t>,
                  std::shared_ptr<sdbusplus::asio::dbus_interface>>
        dbusInterfaceMap;

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface("/xyz/openbmc_project/FruDevice",
                                "xyz.openbmc_project.FruDeviceManager");

    iface->register_method("ReScan", [&]() {
        rescanBusses(busMap, dbusInterfaceMap, unknownBusObjectCount, powerIsOn,
                     addressBlocklist, objServer);
    });

    iface->register_method("ReScanBus", [&](uint16_t bus) {
        rescanOneBus(busMap, bus, dbusInterfaceMap, true, unknownBusObjectCount,
                     powerIsOn, addressBlocklist, objServer);
    });

    iface->register_method("GetRawFru", getFRUInfo);

    iface->register_method(
        "WriteFru", [&](const uint16_t bus, const uint8_t address,
                        const std::vector<uint8_t>& data) {
            if (!writeFRU(bus, address, data))
            {
                throw std::invalid_argument("Invalid Arguments.");
                return;
            }
            // schedule rescan on success
            rescanBusses(busMap, dbusInterfaceMap, unknownBusObjectCount,
                         powerIsOn, addressBlocklist, objServer);
        });
    iface->initialize();

    std::function<void(sdbusplus::message_t & message)> eventHandler =
        [&](sdbusplus::message_t& message) {
            std::string objectName;
            std::flat_map<std::string, std::variant<std::string, bool, int64_t,
                                                    uint64_t, double>>
                values;
            message.read(objectName, values);
            auto findState = values.find("CurrentHostState");
            if (findState != values.end())
            {
                if (std::get<std::string>(findState->second) ==
                    "xyz.openbmc_project.State.Host.HostState.Running")
                {
                    powerIsOn = true;
                }
            }

            if (powerIsOn)
            {
                rescanBusses(busMap, dbusInterfaceMap, unknownBusObjectCount,
                             powerIsOn, addressBlocklist, objServer);
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
                lg2::info("Callback Error {ERR}", "ERR", ec.message());
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
                    {
                        std::string_view name(&iEvent->name[0], iEvent->len);
                        if (name.starts_with("i2c"))
                        {
                            int bus = busStrToInt(name);
                            if (bus < 0)
                            {
                                lg2::error("Could not parse bus {BUS}", "BUS",
                                           name);
                                continue;
                            }
                            int rootBus = getRootBus(bus);
                            if (rootBus >= 0)
                            {
                                rescanOneBus(busMap,
                                             static_cast<uint16_t>(rootBus),
                                             dbusInterfaceMap, false,
                                             unknownBusObjectCount, powerIsOn,
                                             addressBlocklist, objServer);
                            }
                            rescanOneBus(busMap, static_cast<uint16_t>(bus),
                                         dbusInterfaceMap, false,
                                         unknownBusObjectCount, powerIsOn,
                                         addressBlocklist, objServer);
                        }
                    }
                    break;
                    default:
                        break;
                }
                index += sizeof(inotify_event) + iEvent->len;
            }

            dirWatch.async_read_some(boost::asio::buffer(readBuffer),
                                     watchI2cBusses);
        };

    dirWatch.async_read_some(boost::asio::buffer(readBuffer), watchI2cBusses);
    // run the initial scan
    rescanBusses(busMap, dbusInterfaceMap, unknownBusObjectCount, powerIsOn,
                 addressBlocklist, objServer);

    io.run();
    return 0;
}
