#pragma once

#include "fru_utils.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/container/flat_map.hpp>

#include <filesystem>
#include <optional>
#include <set>

class FruDevice
{
  public:
    FruDevice() : utils({busMap}) {}

    BusMap busMap;
    boost::container::flat_map<size_t, std::optional<std::set<size_t>>>
        busBlocklist;

    boost::container::flat_map<std::pair<size_t, size_t>,
                               std::shared_ptr<sdbusplus::asio::dbus_interface>>
        foundDevices;

    boost::container::flat_map<size_t, std::set<size_t>> failedAddresses;
    boost::container::flat_map<size_t, std::set<size_t>> fruAddresses;

    boost::asio::io_context io;

    // utils member with it's own busMap
    FruUtils utils;

    // functions
    void makeProbeInterface(size_t bus, size_t address,
                            sdbusplus::asio::object_server& objServer);

    int getBusFRUs(int file, int first, int last, int bus,
                   std::shared_ptr<DeviceMap> devices, const bool& powerIsOn,
                   sdbusplus::asio::object_server& objServer);

    void loadBlocklist(const char* path);

    void findI2CDevices(const std::vector<std::filesystem::path>& i2cBuses,
                        BusMap& busmap, const bool& powerIsOn,
                        sdbusplus::asio::object_server& objServer);

    void rescanOneBus(
        BusMap& busmap, uint16_t busNum,
        boost::container::flat_map<
            std::pair<size_t, size_t>,
            std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
        bool dbusCall, size_t& unknownBusObjectCount, const bool& powerIsOn,
        sdbusplus::asio::object_server& objServer,
        std::shared_ptr<sdbusplus::asio::connection>& systemBus);

    void rescanBusses(
        BusMap& busmap,
        boost::container::flat_map<
            std::pair<size_t, size_t>,
            std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
        size_t& unknownBusObjectCount, const bool& powerIsOn,
        sdbusplus::asio::object_server& objServer,
        std::shared_ptr<sdbusplus::asio::connection>& systemBus);

    bool updateFRUProperty(
        const std::string& updatePropertyReq, uint32_t bus, uint32_t address,
        const std::string& propertyName,
        boost::container::flat_map<
            std::pair<size_t, size_t>,
            std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
        size_t& unknownBusObjectCount, const bool& powerIsOn,
        sdbusplus::asio::object_server& objServer,
        std::shared_ptr<sdbusplus::asio::connection>& systemBus);

    void addFruObjectToDbus(
        std::vector<uint8_t>& device,
        boost::container::flat_map<
            std::pair<size_t, size_t>,
            std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
        uint32_t bus, uint32_t address, size_t& unknownBusObjectCount,
        const bool& powerIsOn, sdbusplus::asio::object_server& objServer,
        std::shared_ptr<sdbusplus::asio::connection>& systemBus);
};
