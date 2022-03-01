#include "spd/spd.hpp"

#include "spd/ddr5.hpp"

#include <boost/crc.hpp>

#include <iostream>

constexpr const bool debug = false;

DRAMType SPD::getDRAMType(std::span<const uint8_t> spdImage)
{
    if (spdImage.size() <
        offsetof(SPDKeyFields, dramType) +
            sizeof(decltype(std::declval<SPDKeyFields>().dramType)))
    {
        return DRAMType::dramTypeUnknown;
    }
    return byteToDRAMType(spdImage[offsetof(SPDKeyFields, dramType)]);
}

std::optional<uint32_t> SPD::getSPDSize(std::span<const uint8_t> spdImage)
{
    // SPD size info is located in byte 0.
    if (spdImage.size() <
        offsetof(SPDKeyFields, bytesTotal) +
            sizeof(decltype(std::declval<SPDKeyFields>().dramType)))
    {
        return std::nullopt;
    }

    // byte 0: [6:4]
    // 000: Undefined
    // 001: 256 (DDR3 maximum size)
    // 010: 512 (DDR4 maximum size)
    // 011: 1024 (DDR5 maximum size)
    const uint8_t kByteTotal =
        (spdImage[offsetof(SPDKeyFields, bytesTotal)] >> 4) & 0x07;
    if (kByteTotal < 0x01)
    {
        return std::nullopt;
    }
    return (1 << kByteTotal) * 128;
}

std::unique_ptr<SPD> SPD::getFromImage(std::span<const uint8_t> spdImage)
{
    switch (getDRAMType(spdImage))
    {
        case DRAMType::dramTypeDDR1:
        case DRAMType::dramTypeDDR2:
        case DRAMType::dramTypeFBDIMM:
        case DRAMType::dramTypeDDR3:
        case DRAMType::dramTypeDDR4:
            if (debug)
            {
                // std::cerr print uint8_t as an ASCII, so covert it to uint_32
                std::cerr << "Unsupported SPD type: "
                          << static_cast<uint32_t>(getDRAMType(spdImage))
                          << "\n";
            }
            return nullptr;
        case DRAMType::dramTypeDDR5:
            return DDR5SPD::getFromImage(spdImage);
        default:
            return nullptr;
    }
}

uint16_t SPD::calcJedecCRC16(std::span<const uint8_t> data)
{
    // crc_xmodem_t == crc_optimal<16, 0x1021, 0, 0, false, false>
    boost::crc_xmodem_t result;
    result.process_bytes(data.data(), data.size());
    return result.checksum();
}

std::string SPD::dramTypeToStr(DRAMType type)
{
    switch (type)
    {
        case DRAMType::dramTypeDDR1:
            return "DDR1";
        case DRAMType::dramTypeDDR2:
            return "DDR2";
        case DRAMType::dramTypeFBDIMM:
            return "FBDIMM";
        case DRAMType::dramTypeDDR3:
            return "DDR3";
        case DRAMType::dramTypeDDR4:
            return "DDR4";
        case DRAMType::dramTypeDDR5:
            return "DDR5";
        default:
            return "UNKNOWN";
    }
}

DRAMType SPD::byteToDRAMType(uint8_t b)
{
    switch (b)
    {
        case 0x00:
            return DRAMType::dramTypeUnknown;
        case 0x07:
            return DRAMType::dramTypeDDR1;
        case 0x08:
            return DRAMType::dramTypeDDR2;
        case 0x09:
            return DRAMType::dramTypeFBDIMM;
        case 0x0B:
            return DRAMType::dramTypeDDR3;
        case 0x0C:
            return DRAMType::dramTypeDDR4;
        case 0x12:
            return DRAMType::dramTypeDDR5;
        default:
            return DRAMType::dramTypeUnknown;
    }
}

ModuleType SPD::byteToModuleType(uint8_t b)
{
    switch (b)
    {
        case 0x00:
            return ModuleType::moduleTypeUnknown;
        case 0x01:
            return ModuleType::moduleTypeRDIMM;
        case 0x02:
            return ModuleType::moduleTypeUDIMM;
        case 0x03:
            return ModuleType::moduleTypeSODIMM;
        case 0x04:
            return ModuleType::moduleTypeLRDIMM;
        case 0x0A:
            return ModuleType::moduleTypeDDIMM;
        case 0x0B:
            return ModuleType::moduleTypeSolderDown;
        case 0x91:
            return ModuleType::moduleTypeNVNRDIMM;
        case 0x92:
            return ModuleType::moduleTypeNVPRDIMM;
        default:
            return ModuleType::moduleTypeUnknown;
    }
}
