#pragma once

#include "fru_utils.hpp"

#include <boost/asio/io_context.hpp>

#include <filesystem>
#include <flat_set>
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

    std::flat_map<size_t, std::optional<std::flat_set<size_t>>> busBlocklist;

    std::flat_map<std::pair<size_t, size_t>,
                  std::shared_ptr<sdbusplus::asio::dbus_interface>>
        foundDevices;

    std::flat_map<size_t, std::flat_set<size_t>> failedAddresses;
    std::flat_map<size_t, std::flat_set<size_t>> fruAddresses;

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
        std::flat_map<std::pair<size_t, size_t>,
                      std::shared_ptr<sdbusplus::asio::dbus_interface>>&
            dbusInterfaceMap,
        bool dbusCall, size_t& unknownBusObjectCount, const bool& powerIsOn,
        const std::set<size_t>& addressBlocklist,
        sdbusplus::asio::object_server& objServer);

    void rescanBusses(
        std::flat_map<std::pair<size_t, size_t>,
                      std::shared_ptr<sdbusplus::asio::dbus_interface>>&
            dbusInterfaceMap,
        size_t& unknownBusObjectCount, const bool& powerIsOn,
        const std::set<size_t>& addressBlocklist,
        sdbusplus::asio::object_server& objServer);

    bool updateFRUProperty(
        const std::string& propertyValue, uint32_t bus, uint32_t address,
        const std::string& propertyName,
        std::flat_map<std::pair<size_t, size_t>,
                      std::shared_ptr<sdbusplus::asio::dbus_interface>>&
            dbusInterfaceMap,
        size_t& unknownBusObjectCount, const bool& powerIsOn,
        const std::set<size_t>& addressBlocklist,
        sdbusplus::asio::object_server& objServer);

    void addFruObjectToDbus(
        const std::vector<uint8_t>& device,
        std::flat_map<std::pair<size_t, size_t>,
                      std::shared_ptr<sdbusplus::asio::dbus_interface>>&
            dbusInterfaceMap,
        uint32_t bus, uint32_t address, size_t& unknownBusObjectCount,
        const bool& powerIsOn, const std::set<size_t>& addressBlocklist,
        sdbusplus::asio::object_server& objServer);
};
