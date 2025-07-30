#include "inventory_manager.hpp"

#include "system_mapper.hpp"

InventoryManager::InventoryManager(
    std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    std::shared_ptr<sdbusplus::asio::object_server>& objServer,
    SystemMapper& systemMapper) :
    systemMapper(systemMapper), systemBus(systemBus), objServer(objServer)
{}
