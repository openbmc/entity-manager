#pragma once

class EntityManager;

#include "perform_probe.hpp"
#include "perform_scan.hpp"

#include <boost/asio.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/container/flat_set.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <vector>

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

using DBusValueVariant =
    std::variant<std::string, int64_t, uint64_t, double, int32_t, uint32_t,
                 int16_t, uint16_t, uint8_t, bool, std::vector<uint8_t>>;
using DBusInterface = boost::container::flat_map<std::string, DBusValueVariant>;

const std::string objectMapperServiceName = "xyz.openbmc_project.ObjectMapper";
const std::string objectMapperServicePath =
    "/xyz/openbmc_project/object_mapper";
const std::string objectMapperServiceInterface =
    "xyz.openbmc_project.ObjectMapper";
const std::string objectMapperGetSubTreeCmd = "GetSubTree";

constexpr const int32_t maxMapperDepth = 0;

class InventoryManager;

class SystemMapper
{
  public:
    struct DBusInterfaceInstance
    {
        std::string busName;
        std::string path;
        std::string interface;
    };

    SystemMapper(EntityManager& em, boost::asio::io_context& io,
                 std::shared_ptr<sdbusplus::asio::connection>& systemBus,
                 std::shared_ptr<sdbusplus::asio::object_server>& objServer);

    void findDbusObjects(
        std::vector<std::shared_ptr<probe::PerformProbe>>&& probeVector,
        boost::container::flat_set<std::string>&& interfaces,
        size_t retries = 5);

    bool doProbe(const std::vector<std::string>& probeCommand,
                 const std::shared_ptr<scan::PerformScan>& scan,
                 scan::FoundDevices& foundDevs);

    MapperGetSubTreeResponse dbusProbeObjects;

  private:
    EntityManager& entityManager;
    std::shared_ptr<InventoryManager> inventoryManager;
    boost::asio::io_context& io;
    std::shared_ptr<sdbusplus::asio::connection>& systemBus;

    bool probeDbus(const std::string& interfaceName,
                   const std::map<std::string, nlohmann::json>& matches,
                   scan::FoundDevices& devices, bool& foundProbe);

    void getInterfaces(
        const DBusInterfaceInstance& instance,
        const std::vector<std::shared_ptr<probe::PerformProbe>>& probeVector,
        size_t retries = 5);

    void processDbusObjects(
        std::vector<std::shared_ptr<probe::PerformProbe>>& probeVector,
        const GetSubTreeType& interfaceSubtree);
};
