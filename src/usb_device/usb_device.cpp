#include "udev_helpers.hpp"
#include "usb_device_monitor.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <flat_map>
#include <print>
#include <string>
#include <vector>

int main()
{
    std::println("Adding devices...");
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    sdbusplus::asio::object_server objectServer(systemBus,
                                                /*skipManager*/ true);
    objectServer.add_manager("/xyz/openbmc_project/usb_device");
    std::shared_ptr<UsbDeviceMonitor> usbDeviceMonitor =
        std::make_shared<UsbDeviceMonitor>(io, objectServer);
    usbDeviceMonitor->monitorUsbDevice();
    systemBus->request_name("xyz.openbmc_project.UsbDevice");
    io.run();

    return 0;
}
