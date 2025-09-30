#pragma once

#include "fru_utils.hpp"

#include <boost/asio/io_context.hpp>

#include <filesystem>
#include <flat_set>
#include <optional>
#include <set>

// Runtime state shared by the fru-device scan/publish paths. All members
// share the same lifetime (owned by main()) and are always passed together.
struct FruDetails
{
    // this is a map with keys of pair(bus number, address) and values of
    // the object on dbus

    DBusIntfMap dbusInterfaceMap;
    size_t unknownBusObjectCount = 0;
    bool powerIsOn = false;
    std::set<size_t> addressBlocklist;
};

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

    DBusIntfMap foundDevices;

    FruDetails fruDetails;

    std::flat_map<size_t, std::flat_set<size_t>> failedAddresses;
    std::flat_map<size_t, std::flat_set<size_t>> fruAddresses;

    boost::asio::io_context& io;

    FruUtils utils;

    // functions
    void makeProbeInterface(size_t bus, size_t address,
                            sdbusplus::asio::object_server& objServer);

    int getBusFRUs(int file, int first, int last, int bus,
                   sdbusplus::asio::object_server& objServer);

    std::set<size_t> loadBlocklist(const char* path);

    void findI2CDevices(const std::vector<std::filesystem::path>& i2cBuses,
                        sdbusplus::asio::object_server& objServer);

    void rescanOneBus(uint16_t busNum, bool dbusCall,
                      sdbusplus::asio::object_server& objServer);

    void rescanBusses(sdbusplus::asio::object_server& objServer);

    bool updateFRUProperty(const std::string& propertyValue, uint32_t bus,
                           uint32_t address, const std::string& propertyName,
                           sdbusplus::asio::object_server& objServer);

    void addFruObjectToDbus(const std::vector<uint8_t>& device, uint32_t bus,
                            uint32_t address,
                            sdbusplus::asio::object_server& objServer);

    void publishFrusOnBus(uint16_t busNum,
                          sdbusplus::asio::object_server& objServer);
    void publishAllFrus(sdbusplus::asio::object_server& objServer);
};
