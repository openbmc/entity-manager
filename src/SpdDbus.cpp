#include "spd/SpdDbus.hpp"

#include <algorithm>
#include <iomanip>

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
    Dimm::memoryDataWidth(spd->width());
    Dimm::memorySizeInKB(spd->dimmSize() * 1024); // MB -> KB
    Dimm::revisionCode(spd->revisionCode());
    Dimm::memoryTotalWidth(spd->width());

    // auto allSpeeds = spd->speeds();
    // Dimm::allowedSpeedsMT(allSpeeds);
    // Dimm::maxMemorySpeedInMhz(*std::max_element(allSpeeds.begin(),
    // allSpeeds.end()));

    Dimm::Ecc ecc = Dimm::Ecc::NoECC;
    if (spd->eccBits() == 1)
    {
        ecc = Dimm::Ecc::SingleBitECC;
    }
    else if (spd->eccBits() > 1)
    {
        ecc = Dimm::Ecc::MultiBitECC;
    }
    else
    {
        ecc = Dimm::Ecc::NoECC;
    }
    Dimm::ecc(ecc);

    Dimm::DeviceType dt = Dimm::DeviceType::Unknown;
    switch (spd->type())
    {
        case SPD::SPDType::spdTypeDDR1:
            dt = Dimm::DeviceType::DDR;
            break;
        case SPD::SPDType::spdTypeDDR2:
            dt = Dimm::DeviceType::DDR2;
            break;
        case SPD::SPDType::spdTypeFBDIMM:
            dt = Dimm::DeviceType::FBD2;
            break;
        case SPD::SPDType::spdTypeDDR3:
            dt = Dimm::DeviceType::DDR3;
            break;
        case SPD::SPDType::spdTypeDDR4:
            dt = Dimm::DeviceType::DDR4;
            break;
        case SPD::SPDType::spdTypeDDR5:
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
        case SPD::ModuleType::moduleTypeRDIMM:
            ff = Dimm::FormFactor::RDIMM;
            break;
        case SPD::ModuleType::moduleTypeUDIMM:
            ff = Dimm::FormFactor::UDIMM;
            break;
        case SPD::ModuleType::moduleTypeSODIMM:
            ff = Dimm::FormFactor::SO_DIMM;
            break;
        case SPD::ModuleType::moduleTypeLRDIMM:
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
    Asset::manufacturer(std::to_string(spd->manufacturerId().first) + ' ' +
                        std::to_string(spd->manufacturerId().second));
    // TODO: The format of `buildDate` in SPD is YYYYWW, need to convert to
    // YYYYMMDD
    Asset::buildDate(std::to_string(spd->manufacturingDate().year) +
                     std::to_string(spd->manufacturingDate().week));
    Asset::sparePartNumber("");

    auto snVec = spd->serialNumber();
    std::stringstream ss;
    for (auto n : snVec)
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
