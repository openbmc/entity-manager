#include "spd/spd.hpp"

#include "spd/ddr5.hpp"

#include <boost/crc.hpp>

#include <iostream>
#include <span>

Spd::SpdType Spd::getSpdType(std::span<const uint8_t> spdImage)
{
    // Spd type info is located in byte 2.
    if (spdImage.size() < kMinRequiredSpdSize)
    {
        return SpdType::kTypeUnknown;
    }
    return static_cast<SpdType>(spdImage[2]);
}

int32_t Spd::getSpdSize(std::span<const uint8_t> spdImage)
{
    // SPD size info is located in byte 0.
    if (spdImage.size() < kMinRequiredSpdSize)
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

std::unique_ptr<Spd> Spd::getFromImage(std::span<const uint8_t> spdImage)
{
    switch (getSpdType(spdImage))
    {
        case SpdType::kTypeDdr1:
        case SpdType::kTypeDdr2:
        case SpdType::kTypeFbdimm:
        case SpdType::kTypeDdr3:
        case SpdType::kTypeDdr4:
            std::cerr << "Unsupported SPD type: 0x" << std::hex
                      << static_cast<uint8_t>(getSpdType(spdImage)) << "\n";
            return nullptr;
        case SpdType::kTypeDdr5:
            return Ddr5Spd::getFromImage(spdImage);
        default:
            std::cerr << "Unrecognized SPD type: 0x" << std::hex
                      << static_cast<uint8_t>(getSpdType(spdImage)) << "\n";
            return nullptr;
    }
}

uint16_t Spd::calcJedecCrc16(std::span<const uint8_t> data)
{
    // crc_xmodem_t == crc_optimal<16, 0x1021, 0, 0, false, false>
    boost::crc_xmodem_t result;
    result.process_bytes(data.data(), data.size());
    return result.checksum();
}
