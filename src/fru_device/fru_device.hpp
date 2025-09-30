#pragma once

#include "fru_utils.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/container/flat_map.hpp>

#include <filesystem>
#include <optional>
#include <set>

class FruDevice : public std::enable_shared_from_this<FruDevice>
{
  public:
    explicit FruDevice(boost::asio::io_context& ioIn);

    // non-copyable
    FruDevice(const FruDevice&) = delete;
    FruDevice& operator=(const FruDevice&) = delete;
    // non-moveable
    FruDevice(FruDevice&&) = delete;
    FruDevice& operator=(FruDevice&&) = delete;

    boost::container::flat_map<size_t, std::optional<std::set<size_t>>>
        busBlocklist;

    boost::container::flat_map<std::pair<size_t, size_t>,
                               std::shared_ptr<sdbusplus::asio::dbus_interface>>
        foundDevices;

    boost::container::flat_map<size_t, std::set<size_t>> failedAddresses;
    boost::container::flat_map<size_t, std::set<size_t>> fruAddresses;

    boost::asio::io_context& io;

    FruUtils utils;

    // functions
    void makeProbeInterface(size_t bus, size_t address,
                            sdbusplus::asio::object_server& objServer);

    int getBusFRUs(int file, int first, int last, int bus,
                   const bool& powerIsOn,
                   const std::set<size_t>& addressBlocklist,
                   sdbusplus::asio::object_server& objServer);

    std::set<size_t> loadBlocklist(const char* path);

    void findI2CDevices(const std::vector<std::filesystem::path>& i2cBuses,
                        const bool& powerIsOn,
                        const std::set<size_t>& addressBlocklist,
                        sdbusplus::asio::object_server& objServer);

    void rescanOneBus(
        uint16_t busNum,
        boost::container::flat_map<
            std::pair<size_t, size_t>,
            std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
        bool dbusCall, size_t& unknownBusObjectCount, const bool& powerIsOn,
        const std::set<size_t>& addressBlocklist,
        sdbusplus::asio::object_server& objServer);

    void rescanBusses(
        boost::container::flat_map<
            std::pair<size_t, size_t>,
            std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
        size_t& unknownBusObjectCount, const bool& powerIsOn,
        const std::set<size_t>& addressBlocklist,
        sdbusplus::asio::object_server& objServer);

    bool updateFRUProperty(
        const std::string& propertyValue, uint32_t bus, uint32_t address,
        const std::string& propertyName,
        boost::container::flat_map<
            std::pair<size_t, size_t>,
            std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
        size_t& unknownBusObjectCount, const bool& powerIsOn,
        const std::set<size_t>& addressBlocklist,
        sdbusplus::asio::object_server& objServer);

    void addFruObjectToDbus(
        const std::vector<uint8_t>& device,
        boost::container::flat_map<
            std::pair<size_t, size_t>,
            std::shared_ptr<sdbusplus::asio::dbus_interface>>& dbusInterfaceMap,
        uint32_t bus, uint32_t address, size_t& unknownBusObjectCount,
        const bool& powerIsOn, const std::set<size_t>& addressBlocklist,
        sdbusplus::asio::object_server& objServer);
};
