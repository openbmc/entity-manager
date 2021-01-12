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
/// \file IpmbFruDevice.cpp

#include "FruUtils.hpp"
#include "IpmbFruDevice.hpp"
#include "Utils.hpp"

boost::asio::io_service io;
bool powerIsOn = false;

BusMap busMap;

int main()
{
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);
    auto objServer = sdbusplus::asio::object_server(systemBus);

    systemBus->request_name("xyz.openbmc_project.Ipmb.FruDevice");

    boost::asio::deadline_timer configTimer(io);
    const constexpr char* inventoryPath = "/xyz/openbmc_project/inventory";

    // this is a map with keys of pair(bus number, address) and values of
    // the object on dbus
    boost::container::flat_map<std::pair<size_t, size_t>,
                               std::shared_ptr<sdbusplus::asio::dbus_interface>>
        dbusInterfaceMap;

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface("/xyz/openbmc_project/Ipmb/FruDevice",
                                "xyz.openbmc_project.Ipmb.FruDeviceManager");

    iface->register_method("ReScan", [&]() {
        ipmbScanDevices(dbusInterfaceMap, systemBus, objServer);
    });

    iface->register_method("GetRawFru", getFRUInfo);

    iface->register_method(
        "WriteFru", [&](const uint8_t bus, const std::vector<uint8_t>& data) {
            if (!ipmbWriteFru(bus, data, systemBus))
            {
                throw std::invalid_argument("Invalid Arguments.");
            }
            // schedule rescan on success
            ipmbScanDevices(dbusInterfaceMap, systemBus, objServer);
        });

    iface->initialize();

    std::function<void(sdbusplus::message::message&)> eventHandler =
        [&](sdbusplus::message::message&) {
            configTimer.expires_from_now(boost::posix_time::seconds(1));
            // create a timer because normally multiple properties change
            configTimer.async_wait([&](const boost::system::error_code& ec) {
                if (ec == boost::asio::error::operation_aborted)
                {
                    return; // we're being canceled
                }
                // scan the devices
                ipmbScanDevices(dbusInterfaceMap, systemBus, objServer);
            });
        };

    sdbusplus::bus::match::match configMatch(
        static_cast<sdbusplus::bus::bus&>(*systemBus),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            std::string(inventoryPath) + "',arg0namespace='" + configInterface +
            "'",
        eventHandler);

    // run the initial scan
    ipmbScanDevices(dbusInterfaceMap, systemBus, objServer);

    io.run();
    return 0;
}
