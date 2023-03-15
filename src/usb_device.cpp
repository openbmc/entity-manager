#include "config.h"

#include <libudev.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/steady_timer.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <stdplus/print.hpp>

#include <chrono>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

constexpr bool DEBUG = false;
constexpr std::chrono::duration<int64_t, std::milli> INITIAL_WAITTIME =
    std::chrono::milliseconds(10);
constexpr std::chrono::duration<int64_t> WAITTIME = std::chrono::seconds(2);
constexpr std::chrono::duration<int64_t> INITIAL_TIMEOUT =
    std::chrono::minutes(10);
constexpr std::chrono::duration<int64_t> DEBUG_SLEEP_TIME =
    std::chrono::seconds(10);

struct UsbDevice
{
    std::string path;
    std::string SysPath;
    std::string vendorId;
    std::string productId;
    std::string manufacturer;
    std::string product;
    std::string kernel;
    std::string busnum;
    std::string configuration;
};

// VIP, PID, CONFIGURATION, USB ID -> I2C Bus
using UsbMapping = std::unordered_map<
    std::string,
    std::unordered_map<
        std::string,
        std::unordered_map<std::string,
                           std::unordered_map<std::string, uint8_t>>>>;

class ManageUsbDevices
{
  public:
    ManageUsbDevices() : init(true)
    {
        startTime = std::chrono::system_clock::now();

        std::ifstream configJson(USB_DEVICE_CONFIG);
        if (configJson.is_open())
        {
            auto data = nlohmann::json::parse(configJson, nullptr, false);

            try
            {
                usbMapping = data.get<UsbMapping>();
            }
            catch (nlohmann::json::parse_error& ex)
            {
                stdplus::print(stderr, "failed to parese usb config: {}\n",
                               ex.what());
            }
        }
    }

    /**
     * @brief This function enumerates the already connected TTY Devices and
     * extracts the udev properties
     */
    bool EnumerateTtyDevices()
    {
        return EnumerateDevices("tty");
    }

    /**
     * @brief This function enumerates the already connected USB Devices and
     * extracts the udev properties
     */
    bool EnumerateUsbDevices()
    {
        return EnumerateDevices("usb");
    }

    /**
     * @brief Removes the DBus interface created when the usb device is removed
     * so that the object path of the corresponding device is also removed from
     * the DBus
     */
    bool RemoveIfaceForNode(
        const std::string& devNode,
        const std::shared_ptr<sdbusplus::asio::object_server>& objectServer);

    void MonitorUsbDeviceHandler(
        const std::shared_ptr<boost::asio::steady_timer>& timer,
        const std::shared_ptr<sdbusplus::asio::object_server>& objectServer,
        const boost::system::error_code& ec);
    /**
     * @brief Continuously monitors the file descriptors using Udev library to
     * see if any new USB device is connected or any old USB device is
     * disconnected. For a new device a new DBus interface is created.
     */
    void MonitorUsbDevice(
        const std::shared_ptr<boost::asio::steady_timer>& timer,
        const std::shared_ptr<sdbusplus::asio::object_server>& objectServer);

    std::vector<struct UsbDevice> usbDevices;
    std::unordered_map<std::string,
                       std::shared_ptr<sdbusplus::asio::dbus_interface>>
        ifacePtrMap;

    UsbMapping usbMapping;

  private:
    bool EnumerateDevices(std::string_view type);

    void QueueMonitoring(
        const std::shared_ptr<boost::asio::steady_timer>& timer,
        const std::shared_ptr<sdbusplus::asio::object_server>& objectServer)
    {
        if (init &&
            std::chrono::system_clock::now() - startTime > INITIAL_TIMEOUT)
        {
            init = false;
        }
        timer->expires_from_now(init ? INITIAL_WAITTIME : WAITTIME);
        timer->async_wait(
            [this, timer, objectServer](const boost::system::error_code& ec) {
            MonitorUsbDeviceHandler(timer, objectServer, ec);
        });
    }

    std::string GetSysAttrValue(udev_device* dev, std::string_view key)
    {
        const char* value = udev_device_get_sysattr_value(dev, key.data());

        if (value == nullptr)
        {
            return std::string();
        }

        return std::string(value);
    }

    int fd;
    udev_monitor* monitor;
    bool init;
    std::chrono::system_clock::time_point startTime;
};

bool ManageUsbDevices::EnumerateDevices(std::string_view type)
{
    struct udev* udev;
    struct udev_enumerate* enumerate;
    struct udev_list_entry *devices, *devListEntry;
    struct udev_device* dev;

    udev = udev_new();
    if (!udev)
    {
        stdplus::print(stderr, "Can't create udev- USB Device Probe failed\n");
        return false;
    }

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, type.data());
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(devListEntry, devices)
    {
        struct UsbDevice device;
        std::string path = udev_list_entry_get_name(devListEntry);
        dev = udev_device_new_from_syspath(udev, path.c_str());
        if (!dev)
        {
            // Not a USB device skip
            continue;
        }

        device.path = path;
        auto sysPath = udev_device_get_devnode(dev);
        device.SysPath = sysPath == nullptr ? "" : std::string(sysPath);
        if (DEBUG)
        {
            if (device.SysPath.empty())
            {
                stdplus::print(stderr, "Did not find Device Node Path\n");
            }
            else
            {
                stdplus::print(stderr, "Device Node Path: {}\n",
                               device.SysPath);
            }
        }
        device.vendorId = GetSysAttrValue(dev, "idVendor");
        device.productId = GetSysAttrValue(dev, "idProduct");
        device.kernel = (sdbusplus::message::object_path() /
                         path.substr(path.find_last_of("/")))
                            .str;
        device.busnum = GetSysAttrValue(dev, "busnum");
        device.manufacturer = GetSysAttrValue(dev, "manufacturer");
        device.product = GetSysAttrValue(dev, "product");
        device.configuration = GetSysAttrValue(dev, "configuration");

        if (DEBUG)
        {
            stdplus::print(stderr, "device.path: {}\n", device.path);
            stdplus::print(stderr, "device.SysPath: {}\n", device.SysPath);
            stdplus::print(stderr, "device.kernel: {}\n", device.kernel);
            stdplus::print(stderr, "device.busnum: {}\n", device.busnum);
            stdplus::print(stderr, "device.vendorId: {}\n", device.vendorId);
            stdplus::print(stderr, "device.productId: {}\n", device.productId);
            stdplus::print(stderr, "device.manufacturer: {}\n",
                           device.manufacturer);
            stdplus::print(stderr, "device.product: {}\n", device.product);
        }
        udev_device_unref(dev);

        if (!device.vendorId.empty())
        {
            usbDevices.emplace_back(device);
        }
    }
    /* Free the enumerator object */
    udev_enumerate_unref(enumerate);

    udev_unref(udev);
    return true;
}

bool ManageUsbDevices::RemoveIfaceForNode(
    const std::string& devNode,
    const std::shared_ptr<sdbusplus::asio::object_server>& objectServer)
{
    auto it = ifacePtrMap.find(devNode);
    if (it != ifacePtrMap.end())
    {
        std::shared_ptr<sdbusplus::asio::dbus_interface> iface = it->second;
        objectServer->remove_interface(iface);
        stdplus::print(stderr, "USB device removed with dev node: {}\n",
                       devNode);
        ifacePtrMap.erase(devNode);
        return true;
    }
    return false;
}

void ManageUsbDevices::MonitorUsbDeviceHandler(
    const std::shared_ptr<boost::asio::steady_timer>& timer,
    const std::shared_ptr<sdbusplus::asio::object_server>& objectServer,
    const boost::system::error_code& ec)
{
    if (ec == boost::asio::error::operation_aborted)
    {
        stdplus::print(stderr, "MonitorUsbDeviceHandler aborted: {}\n",
                       ec.what());
        return;
    }

    struct udev_device* dev = udev_monitor_receive_device(monitor);
    while (dev != nullptr)
    {
        auto node = udev_device_get_devnode(dev);
        std::string action = udev_device_get_action(dev);
        std::string path = udev_device_get_syspath(dev);

        if (node == nullptr)
        {
            QueueMonitoring(timer, objectServer);
            return;
        }

        if (action == "remove")
        {
            // Remove object path
            if (!RemoveIfaceForNode(node, objectServer))
            {
                stdplus::print(stderr, "No node found for removal\n");
            }
        }
        else if (action == "add")
        {
            std::string vendorId = GetSysAttrValue(dev, "idVendor");
            std::string productId = GetSysAttrValue(dev, "idProduct");
            std::string kernel = (sdbusplus::message::object_path() /
                                  path.substr(path.find_last_of("/")))
                                     .str;
            std::string product = GetSysAttrValue(dev, "product");
            std::string configuration = GetSysAttrValue(dev, "configuration");

            if (DEBUG)
            {
                // if you want to see when a device reboots, the old
                // DBus object path is removed and creates a new DBus
                // object path. 10 seconds is a random number
                std::this_thread::sleep_for(DEBUG_SLEEP_TIME);
            }

            sdbusplus::message::object_path objPath(
                "/xyz/openbmc_project/devices/" + kernel);
            std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
                objectServer->add_interface(objPath,
                                            "xyz.openbmc_project.UsbDevice");
            iface->register_property(
                "PRODUCT", product,
                sdbusplus::asio::PropertyPermission::readOnly);
            iface->register_property(
                "VID", vendorId, sdbusplus::asio::PropertyPermission::readOnly);
            iface->register_property(
                "PID", productId,
                sdbusplus::asio::PropertyPermission::readOnly);
            iface->register_property(
                "CONFIGURATION", configuration,
                sdbusplus::asio::PropertyPermission::readOnly);

            iface->register_property(
                "UDEVID", objPath.filename().substr(1),
                sdbusplus::asio::PropertyPermission::readOnly);
            iface->register_property(
                "USBPORT", node == nullptr ? std::string() : node,
                sdbusplus::asio::PropertyPermission::readOnly);
            iface->register_property(
                "BUS",
                usbMapping[vendorId][productId][configuration]
                          [objPath.filename().substr(1)],
                sdbusplus::asio::PropertyPermission::readOnly);

            iface->initialize();
            ifacePtrMap.emplace(node, std::move(iface));
        }
        udev_device_unref(dev);
        dev = udev_monitor_receive_device(monitor);
    }

    QueueMonitoring(timer, objectServer);
}

void ManageUsbDevices::MonitorUsbDevice(
    const std::shared_ptr<boost::asio::steady_timer>& timer,
    const std::shared_ptr<sdbusplus::asio::object_server>& objectServer)
{
    struct udev* udev;

    udev = udev_new();
    if (!udev)
    {
        stdplus::print(stderr, "Can't create udev- USB Device Probe failed\n");
        return;
    }

    monitor = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(monitor, "usb",
                                                    "usb_device");
    udev_monitor_enable_receiving(monitor);
    stdplus::print(stderr, "Beginning to monitor...\n");

    fd = udev_monitor_get_fd(monitor);
    QueueMonitoring(timer, objectServer);
}

int main()
{
    ManageUsbDevices manager;
    if (!manager.EnumerateUsbDevices())
    {
        stdplus::print(stderr, "Enumerating USB devices failed\n");
    }
    if (!manager.EnumerateTtyDevices())
    {
        stdplus::print(stderr, "Enumerating TTY devices failed\n");
    }

    stdplus::print(stderr, "Adding devices...\n");
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto objectServer =
        std::make_shared<sdbusplus::asio::object_server>(systemBus,
                                                         /*skipManager=*/true);
    objectServer->add_manager("/xyz/openbmc_project/devices");
    for (auto device : manager.usbDevices)
    {
        sdbusplus::message::object_path objPath(
            "/xyz/openbmc_project/devices/" + device.kernel);
        std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
            objectServer->add_interface(objPath,
                                        "xyz.openbmc_project.UsbDevice");
        iface->register_property("PRODUCT", device.product,
                                 sdbusplus::asio::PropertyPermission::readOnly);
        iface->register_property("VID", device.vendorId,
                                 sdbusplus::asio::PropertyPermission::readOnly);
        iface->register_property("PID", device.productId,
                                 sdbusplus::asio::PropertyPermission::readOnly);
        iface->register_property("CONFIGURATION", device.configuration,
                                 sdbusplus::asio::PropertyPermission::readOnly);
        iface->register_property("UDEVID", objPath.filename().substr(1),
                                 sdbusplus::asio::PropertyPermission::readOnly);
        iface->register_property("USBPORT", device.SysPath,
                                 sdbusplus::asio::PropertyPermission::readOnly);
        iface->register_property(
            "BUS",
            manager
                .usbMapping[device.vendorId][device.productId]
                           [device.configuration][objPath.filename().substr(1)],
            sdbusplus::asio::PropertyPermission::readOnly);

        iface->initialize();

        manager.ifacePtrMap.emplace(device.SysPath, std::move(iface));
    }
    systemBus->request_name("xyz.openbmc_project.usbdevice");

    // Monitor
    auto monitorTimer = std::make_shared<boost::asio::steady_timer>(io);
    manager.MonitorUsbDevice(monitorTimer, objectServer);
    io.run();
    return 0;
}
