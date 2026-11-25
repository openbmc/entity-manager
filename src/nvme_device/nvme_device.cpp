// SPDX-License-Identifier: Apache-2.0

#include "../fru_device/fru_reader.hpp"
#include "../fru_device/fru_utils.hpp"
#include "nvme_device_utils.hpp"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/asio/property.hpp>
#include <sdbusplus/bus/match.hpp>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <flat_map>
#include <map>
#include <memory>
#include <set>
#include <variant>

extern "C"
{
#include <i2c/smbus.h>
}

PHOSPHOR_LOG2_USING;

namespace
{

constexpr const char* serviceName = "xyz.openbmc_project.NvmeDevice";
constexpr const char* managerPath = "/xyz/openbmc_project/NvmeDevice";
constexpr const char* managerIntf = "xyz.openbmc_project.NvmeDeviceManager";
constexpr const char* deviceIntf = "xyz.openbmc_project.NvmeDevice";
constexpr const char* runningState =
    "xyz.openbmc_project.State.Host.HostState.Running";

constexpr uint8_t regVendorId = 0x09;
constexpr uint8_t regSerialFirst = 11;
constexpr size_t serialLength = 20;

boost::asio::io_context io;

class Fd
{
  public:
    explicit Fd(int fd) : fd(fd) {}
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    ~Fd()
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }
    int get() const
    {
        return fd;
    }

  private:
    int fd;
};

// Rate-limited error reporting: log only when the message for a (bus, stage)
// changes, and log once on recovery.
class ErrorLog
{
  public:
    void fail(uint16_t bus, uint8_t addr, const std::string& stage,
              const std::string& why)
    {
        auto& last = lastError[{bus, stage}];
        if (last != why)
        {
            last = why;
            error("NVMe bus {BUS} addr {ADDR}: {STAGE} failed: {WHY}", "BUS",
                  bus, "ADDR", lg2::hex, addr, "STAGE", stage, "WHY", why);
        }
    }

    void ok(uint16_t bus, const std::string& stage)
    {
        if (lastError.erase({bus, stage}) != 0)
        {
            info("NVMe bus {BUS}: {STAGE} recovered", "BUS", bus, "STAGE",
                 stage);
        }
    }

  private:
    std::map<std::pair<uint16_t, std::string>, std::string> lastError;
};

ErrorLog errorLog;

std::optional<nvme::Identity> readIdentity(uint16_t bus)
{
    constexpr uint8_t addr = nvme::nvmeMiAddress;
    std::string path = "/dev/i2c-" + std::to_string(bus);
    Fd file(open(path.c_str(), O_RDWR | O_CLOEXEC));
    if (file.get() < 0)
    {
        errorLog.fail(bus, addr, "open", std::strerror(errno));
        return std::nullopt;
    }
    unsigned long funcs = 0;
    constexpr unsigned long needed =
        I2C_FUNC_SMBUS_READ_BYTE_DATA | I2C_FUNC_SMBUS_READ_WORD_DATA;
    if (ioctl(file.get(), I2C_FUNCS, &funcs) < 0 || (funcs & needed) != needed)
    {
        errorLog.fail(bus, addr, "funcs", "adapter lacks SMBus byte/word read");
        return std::nullopt;
    }
    if (ioctl(file.get(), I2C_SLAVE, addr) < 0)
    {
        errorLog.fail(bus, addr, "set-address", std::strerror(errno));
        return std::nullopt;
    }

    int word = i2c_smbus_read_word_data(file.get(), regVendorId);
    if (word < 0)
    {
        errorLog.fail(bus, addr, "read-vendor-id", std::strerror(errno));
        return std::nullopt;
    }
    std::array<uint8_t, serialLength> serial{};
    for (size_t i = 0; i < serialLength; i++)
    {
        int byte = i2c_smbus_read_byte_data(
            file.get(), static_cast<uint8_t>(regSerialFirst + i));
        if (byte < 0)
        {
            errorLog.fail(bus, addr, "read-serial", std::strerror(errno));
            return std::nullopt;
        }
        serial[i] = static_cast<uint8_t>(byte);
    }
    for (const char* stage :
         {"open", "funcs", "set-address", "read-vendor-id", "read-serial"})
    {
        errorLog.ok(bus, stage);
    }

    nvme::Identity id;
    id.vendorId = nvme::decodeVendorId(static_cast<uint16_t>(word));
    id.serial = nvme::trimSerial(serial);
    if (id.serial.empty() &&
        std::any_of(serial.begin(), serial.end(),
                    [](uint8_t c) { return c != 0 && c != ' '; }))
    {
        errorLog.fail(bus, addr, "invalid-serial",
                      "serial has non-printable bytes");
    }
    return id;
}

nvme::VpdResult readVpd(uint16_t bus)
{
    constexpr uint8_t addr = nvme::nvmeVpdAddress;
    std::string path = "/dev/i2c-" + std::to_string(bus);
    Fd file(open(path.c_str(), O_RDWR | O_CLOEXEC));
    if (file.get() < 0)
    {
        errorLog.fail(bus, addr, "vpd-open", std::strerror(errno));
        return std::nullopt;
    }
    if (ioctl(file.get(), I2C_SLAVE, addr) < 0)
    {
        errorLog.fail(bus, addr, "vpd-set-address", std::strerror(errno));
        return std::nullopt;
    }

    bool ioError = false;
    FRUReader reader([&](off_t offset, size_t len, uint8_t* buf) -> int64_t {
        int ret = i2c_smbus_read_i2c_block_data(
            file.get(), static_cast<uint8_t>(offset), len, buf);
        if (ret < 0)
        {
            ioError = true;
        }
        return ret;
    });
    auto contents = readFRUContents(reader, "nvme VPD").first;
    if (contents.empty())
    {
        if (ioError)
        {
            errorLog.fail(bus, addr, "vpd-read", "I2C read error");
            return std::nullopt;
        }
        // Readable, but no valid FRU data: VPD not supported.
        errorLog.ok(bus, "vpd-read");
        return nvme::VpdMap{};
    }

    std::flat_map<std::string, std::string, std::less<>> formatted;
    resCodes rc = formatIPMIFRU(contents, formatted);
    errorLog.ok(bus, "vpd-read");
    nvme::VpdMap out;
    if (rc != resCodes::resErr)
    {
        for (const auto& [key, value] : formatted)
        {
            out[key] = value;
        }
    }
    return out;
}

void readOcp(uint16_t bus, nvme::DeviceState& state)
{
    constexpr uint8_t addr = nvme::nvmeMiAddress;
    std::string path = "/dev/i2c-" + std::to_string(bus);
    Fd file(open(path.c_str(), O_RDWR | O_CLOEXEC));
    if (file.get() < 0)
    {
        errorLog.fail(bus, addr, "ocp-open", std::strerror(errno));
        return;
    }
    errorLog.ok(bus, "ocp-open");
    unsigned long funcs = 0;
    if (ioctl(file.get(), I2C_FUNCS, &funcs) < 0)
    {
        errorLog.fail(bus, addr, "ocp-funcs", std::strerror(errno));
        return;
    }
    if ((funcs & I2C_FUNC_I2C) == 0)
    {
        errorLog.fail(bus, addr, "ocp-funcs", "adapter lacks I2C transfers");
        return;
    }
    errorLog.ok(bus, "ocp-funcs");

    constexpr std::array<std::pair<uint8_t, size_t>, 4> commands = {{
        {nvme::ocpGuidCommand, nvme::ocpGuidPayloadLength},
        {nvme::ocpPowerCapacityCommand, nvme::ocpPowerCapacityPayloadLength},
        {nvme::ocpFirmwareModelCommand, nvme::ocpFirmwareModelPayloadLength},
        {nvme::ocpDataFormatVersionCommand,
         nvme::ocpDataFormatVersionPayloadLength},
    }};
    for (auto [command, length] : commands)
    {
        std::string stage = "ocp-command-" + std::to_string(command);
        std::vector<uint8_t> response(length + 2);
        std::array<i2c_msg, 2> messages = {{
            {addr, 0, 1, &command},
            {addr, I2C_M_RD, static_cast<__u16>(response.size()),
             response.data()},
        }};
        i2c_rdwr_ioctl_data transfer{messages.data(),
                                     static_cast<__u32>(messages.size())};
        int result = ioctl(file.get(), I2C_RDWR, &transfer);
        if (result != static_cast<int>(messages.size()))
        {
            errorLog.fail(bus, addr, stage,
                          result < 0 ? std::strerror(errno) : "short transfer");
            continue;
        }
        auto properties = nvme::decodeOcpResponse(command, response);
        if (!properties)
        {
            errorLog.fail(bus, addr, stage, "invalid length, PEC or data");
            continue;
        }
        state.ocp[command] = std::move(*properties);
        errorLog.ok(bus, stage);
    }
}

struct Device
{
    nvme::ConfigEntry cfg;
    std::optional<nvme::DeviceState> state;
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;
    // What is currently registered on D-Bus.
    nvme::Identity publishedId;
    nvme::VpdMap publishedVpd;
    std::map<uint8_t, nvme::OcpProperties> publishedOcp;
};

class NvmeManager
{
  public:
    NvmeManager(sdbusplus::asio::object_server& objServer,
                std::string configPath) :
        objServer(objServer), configPath(std::move(configPath)), timer(io)
    {}

    // Scans are serialized: everything runs on the single io_context thread.
    void scanAll()
    {
        auto cfg = loadConfigLogged();
        for (auto it = devices.begin(); it != devices.end();)
        {
            bool configured = std::ranges::any_of(
                cfg.entries, [&](const nvme::ConfigEntry& e) {
                    return e == it->second.cfg;
                });
            if (configured)
            {
                ++it;
                continue;
            }
            removeInterface(it->second);
            it = devices.erase(it);
        }
        for (const auto& entry : cfg.entries)
        {
            scanDevice(entry);
        }
    }

    void scanBus(uint16_t bus)
    {
        auto cfg = loadConfigLogged();
        auto it =
            std::ranges::find_if(cfg.entries, [&](const nvme::ConfigEntry& e) {
                return e.bus == bus;
            });
        if (it == cfg.entries.end())
        {
            throw std::invalid_argument("Invalid Bus.");
        }
        scanDevice(*it);
    }

    // Debounce dense requests into a single full scan.
    void scheduleScanAll()
    {
        timer.expires_after(std::chrono::seconds(1));
        timer.async_wait([this](const boost::system::error_code& ec) {
            if (ec == boost::asio::error::operation_aborted)
            {
                return;
            }
            if (ec)
            {
                error("Error in timer: {ERR}", "ERR", ec.message());
                return;
            }
            scanAll();
        });
    }

  private:
    nvme::ConfigResult loadConfigLogged() const
    {
        auto cfg = nvme::loadConfig(configPath);
        for (const auto& msg : cfg.errors)
        {
            error("Config {FILE}: {MSG}", "FILE", configPath, "MSG", msg);
        }
        return cfg;
    }

    void removeInterface(Device& dev)
    {
        if (dev.iface)
        {
            objServer.remove_interface(dev.iface);
            dev.iface.reset();
        }
    }

    void scanDevice(const nvme::ConfigEntry& entry)
    {
        auto& dev = devices[entry.name];
        if (!(dev.cfg == entry))
        {
            // Same name but different bus/index: treat as a new device.
            removeInterface(dev);
            dev = Device{};
            dev.cfg = entry;
        }

        auto identity = readIdentity(entry.bus);
        nvme::applyScan(dev.state, identity,
                        [&]() { return readVpd(entry.bus); });

        if (!dev.state)
        {
            removeInterface(dev);
            devices.erase(entry.name);
            return;
        }
        readOcp(entry.bus, *dev.state);
        try
        {
            publish(dev);
        }
        catch (const std::exception& e)
        {
            error("Failed publishing {PATH}: {ERR}", "PATH", pathFor(entry),
                  "ERR", e);
        }
    }

    static std::string pathFor(const nvme::ConfigEntry& entry)
    {
        return std::string(managerPath) + "/" + entry.name;
    }

    static bool sameKeys(const nvme::VpdMap& a, const nvme::VpdMap& b)
    {
        return a.size() == b.size() &&
               std::ranges::equal(a, b, {}, &nvme::VpdMap::value_type::first,
                                  &nvme::VpdMap::value_type::first);
    }

    void publish(Device& dev)
    {
        const auto& st = *dev.state;

        // Properties cannot be removed from a live interface; rebuild it when
        // the set of VPD properties changes.
        if (dev.iface &&
            (!sameKeys(dev.publishedVpd, st.vpd) ||
             !std::ranges::equal(dev.publishedOcp, st.ocp, {},
                                 &decltype(dev.publishedOcp)::value_type::first,
                                 &decltype(st.ocp)::value_type::first)))
        {
            removeInterface(dev);
        }

        if (!dev.iface)
        {
            using sdbusplus::asio::PropertyPermission;
            auto iface = objServer.add_interface(pathFor(dev.cfg), deviceIntf);
            const auto ro = PropertyPermission::readOnly;
            iface->register_property(
                "ADDRESS", static_cast<uint32_t>(nvme::nvmeMiAddress), ro);
            iface->register_property("BUS", static_cast<uint32_t>(dev.cfg.bus),
                                     ro);
            iface->register_property(
                "VendorID", static_cast<uint32_t>(st.identity.vendorId), ro);
            iface->register_property(
                "VendorName",
                std::string(nvme::vendorName(st.identity.vendorId)), ro);
            iface->register_property("SerialNumber", st.identity.serial, ro);
            iface->register_property("SupportVPD", st.supportVpd(), ro);
            for (const auto& [key, value] : st.vpd)
            {
                iface->register_property(key, value, ro);
            }
            for (const auto& [command, properties] : st.ocp)
            {
                for (const auto& [key, value] : properties)
                {
                    std::visit(
                        [&](const auto& data) {
                            iface->register_property(key, data, ro);
                        },
                        value);
                }
            }
            if (!iface->initialize())
            {
                throw std::runtime_error("interface initialize failed");
            }
            dev.iface = iface;
            dev.publishedId = st.identity;
            dev.publishedVpd = st.vpd;
            dev.publishedOcp = st.ocp;
            return;
        }

        if (!(dev.publishedId == st.identity))
        {
            dev.iface->set_property(
                "VendorID", static_cast<uint32_t>(st.identity.vendorId));
            dev.iface->set_property("VendorName", std::string(nvme::vendorName(
                                                      st.identity.vendorId)));
            dev.iface->set_property("SerialNumber", st.identity.serial);
            dev.publishedId = st.identity;
        }
        for (const auto& [key, value] : st.vpd)
        {
            if (dev.publishedVpd[key] != value)
            {
                dev.iface->set_property(key, value);
            }
        }
        dev.publishedVpd = st.vpd;
        for (const auto& [command, properties] : st.ocp)
        {
            for (const auto& [key, value] : properties)
            {
                if (dev.publishedOcp.at(command).at(key) != value)
                {
                    std::visit(
                        [&](const auto& data) {
                            dev.iface->set_property(key, data);
                        },
                        value);
                }
            }
        }
        dev.publishedOcp = st.ocp;
    }

    sdbusplus::asio::object_server& objServer;
    std::string configPath;
    boost::asio::steady_timer timer;
    std::map<std::string, Device> devices;
};

} // namespace

int main(int argc, char** argv)
{
    std::string configPath = nvme::defaultConfigPath;
    if (argc > 1)
    {
        configPath = argv[1];
    }

    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objServer(systemBus);
    systemBus->request_name(serviceName);

    NvmeManager manager(objServer, configPath);

    auto iface = objServer.add_interface(managerPath, managerIntf);
    iface->register_method("ReScan", [&]() { manager.scheduleScanAll(); });
    iface->register_method("ReScanBus", [&](uint16_t bus) {
        manager.scanBus(bus);
    });
    iface->initialize();

    // Same match as fru-device: only a transition to Running triggers a scan.
    auto onHostState = [&](sdbusplus::message_t& message) {
        std::string objectName;
        std::flat_map<std::string, std::variant<std::string, bool, int64_t,
                                                uint64_t, double>>
            values;
        message.read(objectName, values);
        auto state = values.find("CurrentHostState");
        if (state == values.end())
        {
            return;
        }
        const auto* str = std::get_if<std::string>(&state->second);
        if (str != nullptr && *str == runningState)
        {
            manager.scheduleScanAll();
        }
    };
    sdbusplus::match powerMatch(
        static_cast<sdbusplus::bus_t&>(*systemBus),
        "type='signal',interface='org.freedesktop.DBus.Properties',path='/xyz/"
        "openbmc_project/state/host0',arg0='xyz.openbmc_project.State.Host'",
        onHostState);

    // Initial scan; the current host state is not needed to decide this since
    // a scan always runs at startup.
    manager.scheduleScanAll();

    io.run();
    return 0;
}
