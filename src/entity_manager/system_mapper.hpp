#pragma once

class EntityManager;

#include "object_mapper.hpp"
#include "perform_probe.hpp"
#include "perform_scan.hpp"

#include <boost/asio.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <vector>

class InventoryManager;

class SystemMapper
{
  public:
    SystemMapper(EntityManager& em, boost::asio::io_context& io,
                 std::shared_ptr<sdbusplus::asio::connection>& systemBus,
                 std::shared_ptr<sdbusplus::asio::object_server>& objServer);

    bool doProbe(const std::vector<std::string>& probeCommand,
                 const std::shared_ptr<scan::PerformScan>& scan,
                 scan::FoundDevices& foundDevs);

    void processDbusObjects(
        std::vector<std::shared_ptr<probe::PerformProbe>>& probeVector,
        const GetSubTreeType& interfaceSubtree);

    ObjectMapper objectMapper;
    MapperGetSubTreeResponse dbusProbeObjects;

  private:
    EntityManager& entityManager;
    std::shared_ptr<InventoryManager> inventoryManager;

    bool probeDbus(const std::string& interfaceName,
                   const std::map<std::string, nlohmann::json>& matches,
                   scan::FoundDevices& devices, bool& foundProbe);
};
