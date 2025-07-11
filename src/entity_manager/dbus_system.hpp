#pragma once

#include "perform_probe.hpp"

#include <sdbusplus/asio/connection.hpp>

#include <vector>

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

constexpr const int32_t maxMapperDepth = 0;

class EntityManager;

struct DBusInterfaceInstance
{
    std::string busName;
    std::string path;
    std::string interface;
};

class DbusSystem
{
  public:
    DbusSystem(EntityManager* em,
               std::shared_ptr<sdbusplus::asio::connection>& systemBus);

    void getObjectMapperSubTree(
        std::vector<std::shared_ptr<probe::PerformProbe>>& probes,
        boost::container::flat_set<std::string>& interfaces, GetSubTreeType ret,
        size_t retries = 5);

    void getInterfaces(
        const DBusInterfaceInstance& instance,
        std::vector<std::shared_ptr<probe::PerformProbe>>& probes,
        DBusInterface& ret, size_t retries = 5);

  private:
    EntityManager* _em;
    std::shared_ptr<sdbusplus::asio::connection> bus;
};
