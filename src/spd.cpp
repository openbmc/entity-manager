// Shared functions for parsing SPDs.

#include "spd/spd.hpp"

#include <iostream>
#include <span>

Spd::SpdType Spd::getSpdType(std::span<const uint8_t> spd_image)
{
    // Module type info is located in byte 2 for all supported types.
    if (spd_image.size() < kMinRequiredSpdSize)
    {
        return SpdType::kTypeUnknown;
    }
    return static_cast<SpdType>(spd_image[2]);
}

std::unique_ptr<Spd> Spd::getFromImage(std::span<const uint8_t> spd_image)
{
    switch (getSpdType(spd_image))
    {
        case SpdType::kTypeDdr1:
        case SpdType::kTypeDdr2:
        case SpdType::kTypeDdr3:
        case SpdType::kTypeDdr4:
        case SpdType::kTypeNvm:
        case SpdType::kTypeFbdimm:
        case SpdType::kTypeDdr5:
            std::cerr << "Unsupported SPD type: 0x" << std::hex
                      << static_cast<uint8_t>(getSpdType(spd_image)) << "\n";
            return nullptr;
        default:
            std::cerr << "Unrecognized SPD type: 0x" << std::hex
                      << static_cast<uint8_t>(getSpdType(spd_image)) << "\n";
            return nullptr;
    }
}

uint16_t Spd::calJedecCrc16(std::span<const uint8_t> data)
{
    uint16_t crc = 0;
    for (uint32_t i = 0; i < data.size(); i++)
    {
        crc = crc ^ (static_cast<uint16_t>(data[i]) << 8);
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
