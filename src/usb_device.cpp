#include <libudev.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/asio/io_service.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>
#include <map>
#include <string>
#include <vector>

const bool DEBUG = false;
struct usb_device
{
    std::string path;
    std::string sys_path;
    std::string vendor_id;
    std::string product_id;
    std::string manufacturer;
    std::string product;
    std::string dev_path;
    std::string busnum;
};

std::vector<struct usb_device> usb_devices;
std::map<std::string, std::string> dev_node_map;
std::map<std::string, std::shared_ptr<sdbusplus::asio::dbus_interface>>
    ifacePtrMap;
std::map<std::string, sdbusplus::asio::object_server*> objPtrMap;

int enumerate_usb_devices()
{
    struct udev* udev;
    struct udev_enumerate* enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device* dev;

    udev = udev_new();
    if (!udev)
    {
        std::cerr << "Can't create udev- USB Device Probe failed\n";
        return -1;
    }

    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "tty");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(dev_list_entry, devices)
    {
        struct usb_device device;

        const char* path;
        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);

        device.path = path;
        device.sys_path = udev_device_get_devnode(dev);

        dev = udev_device_get_parent_with_subsystem_devtype(dev, "usb",
                                                            "usb_device");
        if (!dev)
        {
            // Not a USB device skip
            continue;
        }

        if (DEBUG)
        {
            std::cerr << "Device Node Path: " << udev_device_get_devnode(dev)
                      << "\n";
        }
        device.vendor_id = udev_device_get_sysattr_value(dev, "idVendor");
        device.product_id = udev_device_get_sysattr_value(dev, "idProduct");
        device.dev_path = udev_device_get_sysattr_value(dev, "devpath");
        device.busnum = udev_device_get_sysattr_value(dev, "busnum");
        device.manufacturer =
            udev_device_get_sysattr_value(dev, "manufacturer");
        device.product = udev_device_get_sysattr_value(dev, "product");

        if (DEBUG)
        {
            std::cerr << "device.path: " << device.path << "\n";
            std::cerr << "device.sys_path: " << device.sys_path << "\n";
            std::cerr << "device.dev_path: " << device.dev_path << "\n";
            std::cerr << "device.busnum: " << device.busnum << "\n";
            std::cerr << "device.vendor_id: " << device.vendor_id << "\n";
            std::cerr << "device.product_id: " << device.product_id << "\n";
            std::cerr << "device.manufacturer: " << device.manufacturer << "\n";
            std::cerr << "device.product: " << device.product << "\n";
        }

        usb_devices.push_back(device);
        udev_device_unref(dev);
    }
    /* Free the enumerator object */
    udev_enumerate_unref(enumerate);

    udev_unref(udev);
    return 0;
}

int remove_iface_for_node(std::string dev_node,
                          sdbusplus::asio::object_server* objectServer)
{
    auto it = ifacePtrMap.find(dev_node);
    if (it != ifacePtrMap.end())
    {
        std::shared_ptr<sdbusplus::asio::dbus_interface> iface = it->second;
        objectServer->remove_interface(iface);
        std::cout << "USB device removed with dev node: " << dev_node << "\n";
        ifacePtrMap.erase(dev_node);
        return 0;
    }
    return 1;
}

int monitor_usb_device(sdbusplus::asio::object_server* objectServer)
{

    struct udev* udev;
    struct udev_device* dev;

    udev = udev_new();
    if (!udev)
    {
        std::cerr << "Can't create udev- USB Device Probe failed\n";
        return -1;
    }

    auto monitor = udev_monitor_new_from_netlink(udev, "udev");
    udev_monitor_filter_add_match_subsystem_devtype(monitor, "tty", NULL);
    udev_monitor_enable_receiving(monitor);
    std::cout << "Beginning to monitor...\n";
    auto fd = udev_monitor_get_fd(monitor);
    while (1)
    {
        fd_set fds;
        struct timeval tv;
        int ret;

        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        int rc;
        ret = select(fd + 1, &fds, NULL, NULL, &tv);
        /* Check if our file descriptor has received data. */
        if (ret > 0 && FD_ISSET(fd, &fds))
        {
            std::cout
                << "\nselect() says there should be data- USB Device found\n";

            dev = udev_monitor_receive_device(monitor);
            if (dev)
            {
                auto node = udev_device_get_devnode(dev);
                std::string action = udev_device_get_action(dev);

                if (action == "remove")
                {
                    std::cout << "A device was removed with devpath: "
                              << udev_device_get_sysnum(dev) << "\n";
                    // Remove object path
                    rc = remove_iface_for_node(node, objectServer);
                    if (rc)
                    {
                        std::cerr << "No node found for removal\n";
                    }
                }
                else if (action == "add")
                {
                    std::string dev_node = udev_device_get_devnode(dev);
                    dev = udev_device_get_parent_with_subsystem_devtype(
                        dev, "usb", "usb_device");

                    if (DEBUG)
                    {
                        std::cout
                            << "A device was added with these details...\n";
                        std::cout
                            << "idVendor: "
                            << udev_device_get_sysattr_value(dev, "idVendor")
                            << "\n";
                        std::cout
                            << "idProduct: "
                            << udev_device_get_sysattr_value(dev, "idProduct")
                            << "\n";
                        std::cout
                            << "devpath: "
                            << udev_device_get_sysattr_value(dev, "devpath")
                            << "\n";
                        std::cout
                            << "busnum: "
                            << udev_device_get_sysattr_value(dev, "busnum")
                            << "\n";
                        std::cout << "manufacturer: "
                                  << udev_device_get_sysattr_value(
                                         dev, "manufacturer")
                                  << "\n";
                        std::cout
                            << "product: "
                            << udev_device_get_sysattr_value(dev, "product")
                            << "\n";
                    }

                    std::string vendorid =
                        udev_device_get_sysattr_value(dev, "idVendor");
                    std::string busnum =
                        udev_device_get_sysattr_value(dev, "busnum");
                    std::string devpath =
                        udev_device_get_sysattr_value(dev, "devpath");
                    boost::replace_all(devpath, ".", "_");
                    sleep(10); // if you want to see things reboot
                    std::string obj_path =
                        "/xyz/openbmc_project/devices/rde_" + devpath;
                    std::cout << "Adding new device to dbus: " << obj_path
                              << "\n";

                    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
                        objectServer->add_interface(
                            obj_path, "xyz.openbmc_project.UsbDevice");
                    iface->register_property(
                        "VID", vendorid,
                        sdbusplus::asio::PropertyPermission::readOnly);

                    iface->register_property(
                        "UDEVID", busnum + "_" + devpath,
                        sdbusplus::asio::PropertyPermission::readOnly);

                    iface->register_property(
                        "USBPORT", node,
                        sdbusplus::asio::PropertyPermission::readOnly);

                    iface->initialize();
                    ifacePtrMap.insert({dev_node, std::move(iface)});
                }
                udev_device_unref(dev);
            }
            else
            {
                printf("No Device from receive_device(). An error occured.\n");
            }
        }
        sleep(2);
    }
    return 0;
}

int main()
{
    int rc = 0;
    rc = enumerate_usb_devices();
    if (rc)
    {
        std::cerr << "Enumerating USB devices failed";
    }
    std::cout << "Adding devices...\n";
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objectServer(systemBus,
                                                /*skipManager=*/true);
    objectServer.add_manager("/xyz/openbmc_project/devices");
    for (auto device : usb_devices)
    {
        boost::replace_all(device.dev_path, ".", "_");
        std::string obj_path =
            "/xyz/openbmc_project/devices/rde_" + device.dev_path;

        std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
            objectServer.add_interface(obj_path,
                                       "xyz.openbmc_project.UsbDevice");

        iface->register_property("VID", device.vendor_id,
                                 sdbusplus::asio::PropertyPermission::readOnly);
        iface->register_property("UDEVID",
                                 device.busnum + "_" + device.dev_path,
                                 sdbusplus::asio::PropertyPermission::readOnly);
        iface->register_property("USBPORT", device.sys_path,
                                 sdbusplus::asio::PropertyPermission::readOnly);

        iface->initialize();

        ifacePtrMap.insert({device.sys_path, std::move(iface)});
    }
    systemBus->request_name("xyz.openbmc_project.usbdevice");
    std::thread my_thread([&]() { io.run(); });

    // Monitor
    rc = monitor_usb_device(&objectServer);
    my_thread.join();
    return 0;
}
