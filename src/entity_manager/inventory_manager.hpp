#pragma once

#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

class SystemMapper;

class InventoryManager
{
  public:
    InventoryManager(std::shared_ptr<sdbusplus::asio::connection>& systemBus,
                     std::shared_ptr<sdbusplus::asio::object_server>& objServer,
                     SystemMapper& mapper);

    void postToDbus(const nlohmann::json& newConfiguration);
    SystemMapper& systemMapper;

  private:
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
};
