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

#include "FruUtils.hpp"
#include "Utils.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/container/flat_map.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

extern "C"
{
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
}

namespace fs = std::filesystem;

constexpr const char* blacklistPath = PACKAGE_DIR "blacklist.json";

const static constexpr char* i2CDevLocation = "/dev";

boost::asio::io_service io;

void loadBlacklist(const char* path);

int busStrToInt(const std::string& busName);

bool writeFRU(uint8_t bus, uint8_t address, const std::vector<uint8_t>& fru);

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
                rescanBusses(busMap, dbusInterfaceMap, unknownBusObjectCount,
                             powerIsOn, objServer, systemBus);
            }
        };

    sdbusplus::bus::match::match powerMatch = sdbusplus::bus::match::match(
        static_cast<sdbusplus::bus::bus&>(*systemBus),
        "type='signal',interface='org.freedesktop.DBus.Properties',path='/xyz/"
        "openbmc_project/state/"
        "host0',arg0='xyz.openbmc_project.State.Host'",
        eventHandler);

    int fd = inotify_init();
    inotify_add_watch(fd, i2CDevLocation, IN_CREATE | IN_MOVED_TO | IN_DELETE);
    std::array<char, 4096> readBuffer;
    std::string pendingBuffer;
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
            pendingBuffer += std::string(readBuffer.data(), bytesTransferred);
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
                                         dbusInterfaceMap, false,
                                         unknownBusObjectCount, powerIsOn,
                                         objServer, systemBus);
                        }
                }

                pendingBuffer.erase(0, sizeof(inotify_event) + iEvent->len);
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
