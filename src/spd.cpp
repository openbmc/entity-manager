#include "spd/spd.hpp"

#include "spd/ddr5.hpp"

#include <boost/crc.hpp>

#include <iostream>
#include <span>

SPD::SPDType SPD::getSPDType(std::span<const uint8_t> spdImage)
{
    // SPD type info is located in byte 2.
    if (spdImage.size() < kMinRequiredSPDSize)
    {
        return SPDType::spdTypeUnknown;
    }
    return static_cast<SPDType>(spdImage[2]);
}

int32_t SPD::getSPDSize(std::span<const uint8_t> spdImage)
{
    // SPD size info is located in byte 0.
    if (spdImage.size() < kMinRequiredSPDSize)
    {
        return -1;
    }

    const uint8_t kByteTotal = (spdImage[0] >> 4) & 0x07;
    if (kByteTotal > 0x03 || kByteTotal < 0x01)
    {
        return -1;
    }
    return (1 << kByteTotal) * 128;
}

std::unique_ptr<SPD> SPD::getFromImage(std::span<const uint8_t> spdImage)
{
    switch (getSPDType(spdImage))
    {
        case SPDType::spdTypeDDR1:
        case SPDType::spdTypeDDR2:
        case SPDType::spdTypeFBDIMM:
        case SPDType::spdTypeDDR3:
        case SPDType::spdTypeDDR4:
            std::cerr << "Unsupported SPD type: 0x" << std::hex
                      << static_cast<uint8_t>(getSPDType(spdImage)) << "\n";
            return nullptr;
        case SPDType::spdTypeDDR5:
            return DDR5SPD::getFromImage(spdImage);
        default:
            std::cerr << "Unrecognized SPD type: 0x" << std::hex
                      << static_cast<uint8_t>(getSPDType(spdImage)) << "\n";
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

std::string SPD::spdTypeToStr(SPDType type)
{
    switch (type)
    {
        case SPDType::spdTypeDDR1:
            return "DDR1";
        case SPDType::spdTypeDDR2:
            return "DDR2";
        case SPDType::spdTypeFBDIMM:
            return "FBDIMM";
        case SPDType::spdTypeDDR3:
            return "DDR3";
        case SPDType::spdTypeDDR4:
            return "DDR4";
        case SPDType::spdTypeDDR5:
            return "DDR5";
        default:
            return "UNKNOWN";
    }
}
