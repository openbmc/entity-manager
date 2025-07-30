#pragma once

#include "perform_probe.hpp"

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

class SystemMapper;

class ObjectMapper
{
  public:
    struct DBusInterfaceInstance
    {
        std::string busName;
        std::string path;
        std::string interface;
    };

    ObjectMapper(boost::asio::io_context& io,
                 std::shared_ptr<sdbusplus::asio::connection>& systemBus,
                 SystemMapper& systemMapper);

    void getSubTree(
        std::vector<std::shared_ptr<probe::PerformProbe>>&& probeVector,
        boost::container::flat_set<std::string>&& interfaces,
        size_t retries = 5);

    void getInterfaceProperties(
        const DBusInterfaceInstance& instance,
        const std::vector<std::shared_ptr<probe::PerformProbe>>& probeVector,
        MapperGetSubTreeResponse& dbusProbeObjects, size_t retries = 5);

  private:
    boost::asio::io_context& io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    SystemMapper& systemMapper;
};
