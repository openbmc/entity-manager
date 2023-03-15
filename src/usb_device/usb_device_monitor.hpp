#pragma once

#include "udev_helpers.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <flat_map>
#include <print>
#include <string>

class UsbDeviceMonitor : public std::enable_shared_from_this<UsbDeviceMonitor>
{
  public:
    UsbDeviceMonitor(boost::asio::io_context& io,
                     sdbusplus::asio::object_server& objectServer) :
        objectServer(objectServer), udevMonitor(udev.createMonitor()),
        dirwatch(io)
    {
        udevMonitor.filterAddMatchSubsystemDevtype("tty", nullptr);
        udevMonitor.enableReceiving();
        std::println("Beginning to monitor...");
        int fd = udevMonitor.getFd();
        boost::system::error_code ec;
        dirwatch.assign(fd, ec);
        if (ec)
        {
            std::println(
                "Error assigning file descriptor to stream descriptor: {}",
                ec.message());
            return;
        }
    }

    void enumerateUsbDevices()
    {
        UDevEnumerate enumerate = udev.createEnumerate();
        enumerate.addMatchSubsystem("tty");
        enumerate.scanDevices();
        struct udev_list_entry* devices = enumerate.getListEntry();
        struct udev_list_entry* devListEntry = nullptr;
        udev_list_entry_foreach(devListEntry, devices)
        {
            const char* syspath = udev_list_entry_get_name(devListEntry);
            UDevDevice device = udev.createDeviceFromSyspath(syspath);

            createDevice(device);
        }
    }

    void monitorUsbDevice()
    {
        dirwatch.async_wait(boost::asio::posix::stream_descriptor::wait_read,
                            std::bind_front(&UsbDeviceMonitor::onEvent, this,
                                            shared_from_this()));
    }

    void createDevice(UDevDevice& dev)
    {
        std::string vendorid = dev.getSysattrValue("idVendor");
        std::string busnum = dev.getSysattrValue("busnum");
        std::string devpath = dev.getSysattrValue("devpath");
        std::ranges::replace(devpath, '.', '_');
        sdbusplus::message::object_path objPath(
            "/xyz/openbmc_project/usb_device");
        objPath /= devpath;
        std::println("Adding new device to dbus: {}", objPath.str);

        std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
            objectServer.add_interface(objPath.str,
                                       "xyz.openbmc_project.UsbDevice");
        iface->register_property("VID", vendorid);
        std::string udevid = std::format("{}_{}", busnum, devpath);

        iface->register_property("UDEVID", udevid);

        iface->register_property("USBPORT", devpath);

        iface->initialize();
        ifacePtrMap.try_emplace(devpath, std::move(iface));
    }

    void removeDevice(UDevDevice& dev)
    {
        std::string devnode = dev.getDevnode();
        auto it = ifacePtrMap.find(devnode);
        if (it == ifacePtrMap.end())
        {
            std::println("No device found with dev node: {}", devnode);
            return;
        }
        objectServer.remove_interface(it->second);
        std::println("USB device removed with dev node: {}", devnode);
        ifacePtrMap.erase(it);
    }

    void onEvent(const std::shared_ptr<UsbDeviceMonitor>& /*self*/,
                 const boost::system::error_code& ec)
    {
        if (ec)
        {
            std::println("Error monitoring USB device: {}", ec.message());
            return;
        }
        {
            UDevDevice dev = udevMonitor.receiveDevice();
            std::string devnode = dev.getDevnode();
            std::string action = dev.getAction();

            if (action == "remove")
            {
                std::println("A device was removed with devpath: {}",
                             dev.getSysnum());
                removeDevice(dev);
            }
            else if (action == "add")
            {
                std::println("A device was added with devpath: {}",
                             dev.getSysnum());
                createDevice(dev);
            }
            else
            {
                std::println("An unknown event occurred with devpath: {}",
                             devnode);
            }
        }

        monitorUsbDevice();
    }
    std::flat_map<std::string, std::shared_ptr<sdbusplus::asio::dbus_interface>,
                  std::less<>>
        ifacePtrMap;

    UDev udev;

    sdbusplus::asio::object_server& objectServer;
    UDevMonitor udevMonitor;
    boost::asio::posix::stream_descriptor dirwatch;
};
