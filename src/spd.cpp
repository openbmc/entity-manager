// Shared functions for parsing SPDs.

#include "spd/spd.hpp"

#include <iostream>
#include <span>

Spd::SpdType Spd::getSpdType(std::span<const uint8_t> spdImage)
{
    // Module type info is located in byte 2 for all supported types.
    if (spdImage.size() < kMinRequiredSpdSize)
    {
        return SpdType::kTypeUnknown;
    }
    return static_cast<SpdType>(spdImage[2]);
}

std::unique_ptr<Spd> Spd::getFromImage(std::span<const uint8_t> spdImage)
{
    switch (getSpdType(spdImage))
    {
        case SpdType::kTypeDdr1:
        case SpdType::kTypeDdr2:
        case SpdType::kTypeDdr3:
        case SpdType::kTypeDdr4:
        case SpdType::kTypeNvm:
        case SpdType::kTypeFbdimm:
        case SpdType::kTypeDdr5:
            std::cerr << "Unsupported SPD type: 0x" << std::hex
                      << static_cast<uint8_t>(getSpdType(spdImage)) << "\n";
            return nullptr;
        default:
            std::cerr << "Unrecognized SPD type: 0x" << std::hex
                      << static_cast<uint8_t>(getSpdType(spdImage)) << "\n";
            return nullptr;
    }
}

uint16_t Spd::calJedecCrc16(std::span<const uint8_t> data)
{
    uint16_t crc = 0;
    for (uint8_t d : data)
    {
        crc = crc ^ (static_cast<uint16_t>(d) << 8);
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x8000)
            {
                crc = crc << 1 ^ 0x1021;
            }
            else
            {
                crc = crc << 1;
            }
        }
    }

    return crc;
}
