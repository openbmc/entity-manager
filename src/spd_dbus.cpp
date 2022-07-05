#include "spd_dbus.hpp"

#include <algorithm>
#include <optional>

using Dimm = sdbusplus::xyz::openbmc_project::Inventory::Item::server::Dimm;
using Asset =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::Asset;
using LocationCode =
    sdbusplus::xyz::openbmc_project::Inventory::Decorator::server::LocationCode;
using Item = sdbusplus::xyz::openbmc_project::Inventory::server::Item;
using OperationalStatus = sdbusplus::xyz::openbmc_project::State::Decorator::
    server::OperationalStatus;

SpdDbus::SpdDbus(uint32_t bus, uint32_t address, sdbusplus::bus::bus& dbus,
                 sdbusplus::asio::object_server& objServer,
                 const std::string& objPath, std::unique_ptr<SPD>& spd) :
    sdbusplus::server::object::object<Dimm>(dbus, objPath.c_str()),
    sdbusplus::server::object::object<Asset>(dbus, objPath.c_str()),
    sdbusplus::server::object::object<LocationCode>(dbus, objPath.c_str()),
    sdbusplus::server::object::object<Item>(dbus, objPath.c_str()),
    sdbusplus::server::object::object<OperationalStatus>(dbus, objPath.c_str())
{
    // xyz/openbmc_project/Inventory/Item
    Item::present(true);

    // xyz/openbmc_project/Inventory/Item/Dimm
    Dimm::memoryDataWidth(spd->width().value_or(0));
    Dimm::memorySizeInKB(spd->dimmSizeKB().value_or(0));
    Dimm::memoryType(spd->deviceType());

    auto allSpeeds = spd->speeds();
    Dimm::allowedSpeedsMT(allSpeeds);
    Dimm::maxMemorySpeedInMhz(
        allSpeeds.empty()
            ? 0
            : *std::max_element(allSpeeds.begin(), allSpeeds.end()));

    Dimm::memoryAttributes(spd->groupNum().value_or(0));
    Dimm::ecc(spd->ecc());

    auto allLatencies = spd->casLatencies();
    // Return minimum latency
    Dimm::casLatencies(
        allLatencies.empty()
            ? 0
            : *std::min_element(allLatencies.begin(), allLatencies.end()));

    Dimm::revisionCode(spd->revisionCode());
    // FormFactor doesn't have `unknown/other` type, so set the defualt form
    // factor to RDIMM.
    Dimm::formFactor(spd->moduleType().value_or(Dimm::FormFactor::RDIMM));
    Dimm::memoryMedia(spd->tech());

    // xyz/openbmc_project/Inventory/Decorator/Asset
    Asset::partNumber(spd->partNumber());
    Asset::sparePartNumber("");
    Asset::serialNumber(spd->serialNumber());
    Asset::manufacturer(spd->manufacturerId().value_or(""));

    // The format of `buildDate` in SPD is YYYYWW, need to convert to YYYYMMDD
    char date[] = "YYYYMMDD";
    if (spd->manufacturingDate() != std::nullopt)
    {
        auto tt =
            std::chrono::system_clock::to_time_t(*spd->manufacturingDate());
        auto tm = gmtime(&tt);
        strftime(date, sizeof(date), "%Y%m%d", tm);
    }
    Asset::buildDate(date);

    // xyz/openbmc_project/State/Decorator/OperationalStatus
    OperationalStatus::functional(true);

    // i2cdevice
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        objServer.add_interface(objPath.c_str(),
                                "xyz.openbmc_project.Inventory.Item.I2CDevice");
    iface->register_property("BUS", bus);
    iface->register_property("ADDRESS", address);
    iface->initialize();
}
