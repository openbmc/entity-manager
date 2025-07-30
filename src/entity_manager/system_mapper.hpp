#pragma once

class EntityManager;

#include "configuration.hpp"
#include "inventory_manager.hpp"
#include "object_mapper.hpp"
#include "perform_probe.hpp"
#include "perform_scan.hpp"
#include "topology.hpp"

#include <boost/asio.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <vector>

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

using DBusValueVariant =
    std::variant<std::string, int64_t, uint64_t, double, int32_t, uint32_t,
                 int16_t, uint16_t, uint8_t, bool, std::vector<uint8_t>>;
using DBusInterface = boost::container::flat_map<std::string, DBusValueVariant>;

class SystemMapper
{
  public:
    SystemMapper(EntityManager& em, boost::asio::io_context& io,
                 std::shared_ptr<sdbusplus::asio::connection>& systemBus,
                 std::shared_ptr<sdbusplus::asio::object_server>& objServer);

    // disable copy
    SystemMapper(const SystemMapper&) = delete;
    SystemMapper& operator=(const SystemMapper&) = delete;

    // disable move
    SystemMapper(SystemMapper&&) = delete;
    SystemMapper& operator=(SystemMapper&&) = delete;

    bool doProbe(const std::vector<std::string>& probeCommand,
                 const std::shared_ptr<scan::PerformScan>& scan,
                 scan::FoundDevices& foundDevs);

    Configuration configuration;
    InventoryManager inventoryManager;
    nlohmann::json lastJson;
    nlohmann::json systemConfiguration;
    Topology topology;

    void processDbusObjects(
        std::vector<std::shared_ptr<probe::PerformProbe>>& probeVector,
        const GetSubTreeType& interfaceSubtree);

    ObjectMapper objectMapper;
    MapperGetSubTreeResponse dbusProbeObjects;

  private:
    EntityManager& entityManager;
    boost::asio::io_context& io;
    std::shared_ptr<sdbusplus::asio::connection>& systemBus;

    bool probeDbus(const std::string& interfaceName,
                   const std::map<std::string, nlohmann::json>& matches,
                   scan::FoundDevices& devices, bool& foundProbe);
};
