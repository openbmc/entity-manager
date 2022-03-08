#include "spd/SpdDbus.hpp"

#include <algorithm>
#include <iomanip>
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
    Dimm::memorySizeInKB(spd->dimmSizeMiB().value_or(0) * 1024); // MB -> KB
    Dimm::revisionCode(spd->revisionCode());
    Dimm::memoryTotalWidth(spd->width().value_or(0));

    auto allSpeeds = spd->speeds();
    Dimm::allowedSpeedsMT(allSpeeds);
    Dimm::maxMemorySpeedInMhz(allSpeeds.empty() ? 0 : *std::max_element(allSpeeds.begin(), allSpeeds.end()));

    Dimm::Ecc ecc = Dimm::Ecc::NoECC;
    if (spd->eccBits() == std::nullopt) {
        ecc = Dimm::Ecc::NoECC;
    } else if (*spd->eccBits() == 1)
    {
        ecc = Dimm::Ecc::SingleBitECC;
    }
    else if (*spd->eccBits() > 1)
    {
        ecc = Dimm::Ecc::MultiBitECC;
    }
    Dimm::ecc(ecc);

    Dimm::DeviceType dt = Dimm::DeviceType::Unknown;
    switch (spd->type())
    {
        case DRAMType::dramTypeDDR1:
            dt = Dimm::DeviceType::DDR;
            break;
        case DRAMType::dramTypeDDR2:
            dt = Dimm::DeviceType::DDR2;
            break;
        case DRAMType::dramTypeFBDIMM:
            dt = Dimm::DeviceType::FBD2;
            break;
        case DRAMType::dramTypeDDR3:
            dt = Dimm::DeviceType::DDR3;
            break;
        case DRAMType::dramTypeDDR4:
            dt = Dimm::DeviceType::DDR4;
            break;
        case DRAMType::dramTypeDDR5:
            dt = Dimm::DeviceType::DDR5;
            break;
        default:
            dt = Dimm::DeviceType::Unknown;
            break;
    }
    Dimm::memoryType(dt);

    Dimm::FormFactor ff = Dimm::FormFactor::RDIMM;
    switch (spd->module())
    {
        case ModuleType::moduleTypeRDIMM:
            ff = Dimm::FormFactor::RDIMM;
            break;
        case ModuleType::moduleTypeUDIMM:
            ff = Dimm::FormFactor::UDIMM;
            break;
        case ModuleType::moduleTypeSODIMM:
            ff = Dimm::FormFactor::SO_DIMM;
            break;
        case ModuleType::moduleTypeLRDIMM:
            ff = Dimm::FormFactor::LRDIMM;
            break;
        default:
            // Since the `FormFactor` doesn't have `Unknown`, any unregonized
            // form factor will be treated as `RDIMM`.
            ff = Dimm::FormFactor::RDIMM;
            break;
    }
    Dimm::formFactor(ff);

    // xyz/openbmc_project/Inventory/Decorator/Asset
    Asset::partNumber(spd->partNumber());
    // The result == "bank number" + " " + "offset in bank"
    // Please referring to JEP106 for the definition of `bank` and `offset in
    // bank
    // (TODO): It would be better if we have a JEP106 lookup table. So we can find out the real manufacturer name instead of 2 numbers.
    Asset::manufacturer(spd->manufacturerId() == std::nullopt ? "" : std::to_string(spd->manufacturerId()->bankNumber) + ' ' + std::to_string(spd->manufacturerId()->OffsetInBank));
    // The format of `buildDate` in SPD is YYYYWW, need to convert to YYYYMMDD
    char date[] = "YYYYMMDD";
    if (spd->manufacturingDate() != std::nullopt) {
        auto tt = std::chrono::system_clock::to_time_t(*spd->manufacturingDate());
        auto tm = gmtime(&tt);
        strftime(date, sizeof(date), "%Y%m%d", tm); 
    }
    Asset::buildDate(date);
    Asset::sparePartNumber("");

    auto sn = spd->serialNumber();
    std::stringstream ss;
    for (auto n : sn)
    {
        ss << std::setfill('0') << std::setw(2) << std::hex << n;
    }
    Asset::serialNumber(ss.str());

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
