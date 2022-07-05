#pragma once
#include "spd.hpp"

#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/server.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/LocationCode/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/Dimm/server.hpp>
#include <xyz/openbmc_project/Inventory/Item/server.hpp>
#include <xyz/openbmc_project/State/Decorator/OperationalStatus/server.hpp>

#include <string>

class SpdDbus :
    sdbusplus::server::object::object<
        sdbusplus::xyz::openbmc_project::Inventory::Item::server::Dimm>,
    sdbusplus::server::object::object<
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset>,
    sdbusplus::server::object::object<
        sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::
            LocationCode>,
    sdbusplus::server::object::object<
        sdbusplus::xyz::openbmc_project::Inventory::server::Item>,
    sdbusplus::server::object::object<sdbusplus::xyz::openbmc_project::State::
                                          Decorator::server::OperationalStatus>
{
  public:
    SpdDbus(uint32_t bus, uint32_t address, sdbusplus::bus::bus& dbus,
            sdbusplus::asio::object_server& objServer,
            const std::string& objPath, std::unique_ptr<SPD>& spd);
    ~SpdDbus() = default;
};
