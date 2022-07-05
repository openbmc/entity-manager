#include "spd.hpp"

#include "fru_utils.hpp"
#include "manufacturer_id_table.hpp"

#include <boost/crc.hpp>

#include <cctype>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

const static bool debug = true;

// public static functions
std::unique_ptr<SPD> SPD::parseSPD(std::span<const uint8_t> spdImage)
{
    if (debug)
    {
        std::cerr
            << "gpgpgp: "
            << "spdImage.size() = " << spdImage.size()
            << ", spdSize(spdImage[0]) = "
            << spdSize(spdImage[offsetof(SPDKeyFields, spdSize)]).value_or(9999)
            << ", offsetof(SPDKeyFields, deviceType) = "
            << offsetof(SPDKeyFields, deviceType) << std::endl;
    }
    if (spdImage.size() < kMinRequiredSPDSize ||
        spdSize(spdImage[offsetof(SPDKeyFields, spdSize)]).value_or(0) !=
            spdImage.size() ||
        !isSupportedType(
            deviceType(spdImage[offsetof(SPDKeyFields, deviceType)])))
    {

        return nullptr;
    }

    Dimm::DeviceType type =
        deviceType(spdImage[offsetof(SPDKeyFields, deviceType)]);
    // Different type of dimm stores crc in different position.
    // DDR5: byte 510, 511
    // DDR4: byte 126, 127
    if (type == Dimm::DeviceType::DDR5)
    {
        if (spdSize(spdImage[offsetof(SPDKeyFields, spdSize)]).value_or(0) <
            offsetof(DDR5Fields, crc) +
                sizeof(decltype(std::declval<DDR5Fields>().crc)))
        {
            return nullptr;
        }
        uint16_t storedCrc = spdImage[offsetof(DDR5Fields, crc)] |
                             (spdImage[offsetof(DDR5Fields, crc) + 1] << 8);
        if (calcJedecCRC16(std::span<const uint8_t>{
                spdImage.data(), offsetof(DDR5Fields, crc)}) != storedCrc)
        {
            if (debug)
            {
                std::cerr << "crc mismatch: calculated_crc = "
                          << calcJedecCRC16(std::span<const uint8_t>{
                                 spdImage.data(), offsetof(DDR5Fields, crc)})
                          << ", stored_crc = " << storedCrc << std::endl;
            }
            return nullptr;
        }
    }

    if (debug)
    {
        std::cerr << "gpgpgp: parseSPD done" << std::endl;
    }
    // "return std::make_unique<SPD>(spdImage);" won't work because the
    // constructor is private. make_unique cannot access private constructor.
    return std::unique_ptr<SPD>(new SPD(spdImage));
}

uint16_t SPD::calcJedecCRC16(std::span<const uint8_t> data)
{
    // crc_xmodem_t == crc_optimal<16, 0x1021, 0, 0, false, false>
    boost::crc_xmodem_t result;
    result.process_bytes(data.data(), data.size());
    return result.checksum();
}

bool SPD::isSupportedType(Dimm::DeviceType type)
{
    if (type == Dimm::DeviceType::DDR5)
    {
        return true;
    }
    return false;
}

std::optional<uint32_t> SPD::spdSize(uint8_t byte)
{
    // byte: [6:4]
    // 000: Undefined
    // 001: 256 (DDR3 maximum size)
    // 010: 512 (DDR4 maximum size)
    // 011: 1024 (DDR5 maximum size)
    const uint8_t kTotal = (byte >> 4) & 0x07;
    if (kTotal < 0x01 || kTotal > 0x03)
    {
        return std::nullopt;
    }
    return (1 << kTotal) * 128;
}

Dimm::DeviceType SPD::deviceType(uint8_t byte)
{
    if (!deviceTypeMap.contains(byte))
    {
        return Dimm::DeviceType::Unknown;
    }

    return deviceTypeMap.at(byte);
}

std::optional<Dimm::FormFactor> SPD::moduleType(uint8_t byte)
{
    const uint8_t kModule = byte & 0x0F;
    if (!formFactorMap.contains(kModule))
    {
        return std::nullopt;
    }

    return formFactorMap.at(kModule);
}

// public member functions
uint16_t SPD::calcJedecCRC16() const
{
    return calcJedecCRC16(
        std::span<const uint8_t>{spd_.raw, offsetof(DDR5Fields, crc)});
}

std::optional<uint32_t> SPD::spdSize() const
{
    return spdSize(spd_.ddr5.bytesTotal);
}

Dimm::DeviceType SPD::deviceType() const
{
    return deviceType(spd_.ddr5.hostBusCommandProtocolType);
}

std::optional<Dimm::FormFactor> SPD::moduleType() const
{
    return moduleType(spd_.ddr5.moduleType);
}

std::optional<uint64_t> SPD::dimmSizeKB() const
{
    const auto kPrimaryBusWidthPerChannel = primaryBusWidthPerChannel();
    const auto kWidth = width();
    const auto kNumChannelsPerDimm = numChannelsPerDimm();
    const auto kSdramDensityPerDie = sdramDensityPerDie();
    const auto kDiePerPackage1 = diePerPackage(true);

    if (kPrimaryBusWidthPerChannel == std::nullopt || kWidth == std::nullopt ||
        kNumChannelsPerDimm == std::nullopt ||
        kSdramDensityPerDie == std::nullopt || kDiePerPackage1 == std::nullopt)
    {
        if (debug)
        {
            std::cerr << "Can't calculate dimmSizeKB" << std::endl;
        }
        return std::nullopt;
    }

    uint64_t result = *kNumChannelsPerDimm * *kPrimaryBusWidthPerChannel /
                      *kWidth * *kSdramDensityPerDie / 8;
    if (symmetrical())
    {
        // To calculate the total capacity in bytes for a symmetric module, the
        // following math applies: Capacity in bytes = Number of channels per
        // DIMM * Primary bus width per channel / SDRAM I/O Width * Die per
        // package * SDRAM density per die / 8 * Package ranks per channel
        const uint8_t kNumPackageRanksPerChannel = numPackageRanksPerChannel();
        result *= *kDiePerPackage1 * kNumPackageRanksPerChannel;
    }
    else
    {
        // To calculate the total capacity in bytes for an asymmetric module,
        // the following math applies: Capacity in bytes = Capacity of even
        // ranks (first SDRAM type) + Capacity of odd ranks (second SDRAM type)
        const auto kDiePerPackage2 = diePerPackage(false);
        if (kDiePerPackage2 == std::nullopt)
        {
            if (debug)
            {
                std::cerr
                    << "Can't calculate dimmSizeKB, kDiePerPackage2 is missing"
                    << std::endl;
            }
            return std::nullopt;
        }
        result *= (*kDiePerPackage1 + *kDiePerPackage2);
    }

    // The calculation result is in unit of GiB, but the output is expected
    // to be in unit of KB. So left shift 20 bits.
    result <<= 20;
    return result;
}

std::optional<uint8_t> SPD::width() const
{
    // DDR5 spec
    // byte 6: [7:5]
    // 000: x4
    // 001: x8
    // 010: x16
    // 011: x32
    // All others reserved
    const int8_t kWidth = (spd_.ddr5.sdram1IoWidth >> 5) & 0x07;
    if (kWidth > 0x03)
    {
        if (debug)
        {
            std::cerr << "Number of `IO width` is invalid" << std::endl;
        }
        return std::nullopt;
    }

    return 4 * (1 << kWidth);
}

std::vector<uint16_t> SPD::speeds() const
{
    // Currently only 2 speed are supported.
    // https://en.wikipedia.org/wiki/DDR5_SDRAM
    const std::array<uint16_t, 2> possibleMhz = {4800, 7200};
    std::vector<uint16_t> results;

    double maxSpeed = std::floor(2000000.0 / spd_.ddr5.t_ckavgmin); // MHz
    double minSpeed = std::ceil(2000000.0 / spd_.ddr5.t_ckavgmax);  // MHz

    for (uint16_t s : possibleMhz)
    {
        if (s <= maxSpeed && s >= minSpeed)
        {
            results.emplace_back(s);
        }
    }

    return results;
}

std::optional<uint16_t> SPD::maxClockSpeed() const
{
    auto allSpeeds = speeds();
    if (allSpeeds.empty())
    {
        return std::nullopt;
    }

    return *std::max_element(allSpeeds.begin(), allSpeeds.end());
}

std::optional<uint8_t> SPD::groupNum() const
{
    // DDR5 spec
    // byte 7:  [7:5]
    // 000: 1 bank group
    // 001: 2 bank groups
    // 010: 4 bank groups
    // 011: 8 bank groups
    // All others reserved
    const uint8_t kBankGroups =
        (spd_.ddr5.sdram1BankGroupAndBanksPerBankGroup >> 5) & 0x07;
    if (kBankGroups > 0x03)
    {
        if (debug)
        {
            std::cerr << "Number of `banks` is invalid" << std::endl;
        }
        return std::nullopt;
    }

    return 1 << kBankGroups;
}

Dimm::Ecc SPD::ecc() const
{
    const int8_t kECC = spd_.ddr5.memoryChannelBusWidth & 0x18;
    if (!eccMap.contains(kECC))
    {
        return Dimm::Ecc::NoECC;
    }

    return eccMap.at(kECC);
}

std::vector<uint16_t> SPD::casLatencies() const
{
    /*
        DDR5 spec
        SDRAM CAS Latencies Supported
        Byte 24, bit:  7  6  5  4  3  2  1  0
        CL =          34 32 30 28 26 24 22 20
        Byte 25, bit:  7  6  5  4  3  2  1  0
        CL =          50 48 46 44 42 40 38 36
        Byte 26, bit:  7  6  5  4  3  2  1  0
        CL =          66 64 62 60 58 56 54 52
        Byte 27, bit:  7  6  5  4  3  2  1  0
        CL =          82 80 78 76 74 72 70 68
        Byte 28, bit:  7  6  5  4  3  2  1  0
        CL =          98 96 94 92 90 88 86 84
    */
    const uint16_t kBase = 20;
    const uint16_t kStep = 2;
    std::vector<uint16_t> result;

    for (size_t i = 0; i < sizeof(spd_.ddr5.cas); i++)
    {
        for (size_t j = 0; j < 8; j++)
        {
            if ((spd_.ddr5.cas[i] & (1 << j)) != 0)
            {
                result.push_back((i * 8 + j) * kStep + kBase);
            }
        }
    }

    return result;
}

uint8_t SPD::revisionCode() const
{
    return spd_.ddr5.moduleRevisionCode;
}

Dimm::MemoryTech SPD::tech() const
{
    const int8_t kModule = spd_.ddr5.moduleType & 0xF0;
    if (!memoryTechMap.contains(kModule))
    {
        return Dimm::MemoryTech::Other;
    }

    return memoryTechMap.at(kModule);
};

std::optional<std::string> SPD::manufacturerId() const
{
    if (!manufacturerID.contains(spd_.ddr5.dramManufacturerId))
    {
        return std::nullopt;
    }

    return manufacturerID.at(spd_.ddr5.dramManufacturerId);
}

uint8_t SPD::manufacturerLocation() const
{
    return spd_.ddr5.moduleManufacturingLocation;
}

std::optional<std::chrono::time_point<std::chrono::system_clock>>
    SPD::manufacturingDate() const
{
    auto yy = bcdToInt8(spd_.ddr5.moduleManufacturingDate[0]);
    auto ww = bcdToInt8(spd_.ddr5.moduleManufacturingDate[1]);
    if (yy == std::nullopt || ww == std::nullopt)
    {
        return std::nullopt;
    }
    *yy += 2000; // yy is the year after 2000. Defined by SPD spec.

    std::tm tmYearStart;
    tmYearStart.tm_mday = 1;
    tmYearStart.tm_mon = 0;
    tmYearStart.tm_year = *yy - 1900;
    tmYearStart.tm_isdst = 0;

    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tmYearStart));
    std::chrono::weeks week(*ww - 1); // ww is 1-indexed
    tp += week;

    return tp;
}

std::string SPD::serialNumber() const
{
    std::stringstream result;
    result << "0x";
    for (uint8_t c : spd_.ddr5.serialNumber)
    {
        result << std::setfill('0') << std::setw(2) << std::hex
               << static_cast<uint16_t>(c);
    }
    return result.str();
}

std::string SPD::partNumber() const
{
    std::string result;
    for (uint8_t c : spd_.ddr5.partNumber)
    {
        if (std::isprint(c) && !std::isspace(c))
        {
            result.push_back(c);
        }
    }
    return result;
}

uint16_t SPD::checksum() const
{
    return spd_.ddr5.crc;
}

std::span<const uint8_t> SPD::raw() const
{
    return std::span<const uint8_t>{spd_.raw};
}

// private functions
SPD::SPD(std::span<const uint8_t> spdImage)
{
    // Supposedly we don’t need to check size/content of `spdImage` here since
    // the `parseSPD` already checks it.
    size_t len = std::min(sizeof(spd_.raw), spdImage.size());
    memcpy(spd_.raw, spdImage.data(), len);
    // set the remaining data to 0
    memset(spd_.raw + len, 0, sizeof(spd_.raw) - len);
}

bool SPD::symmetrical() const
{
    // DDR5 spec
    // byte 234: [6]
    // 0: Symmetrical
    // 1: Asymmetrical
    return (spd_.ddr5.moduleOrganization & 0x40) == 0;
}

std::optional<uint8_t> SPD::primaryBusWidthPerChannel() const
{
    // DDR5 spec
    // byte 235 [2:0]
    // 000 = 8 bits
    // 001 = 16 bits
    // 010 = 32 bits
    // 011 = 64 bits
    // All others reserved
    const uint8_t kPrimaryBusWidthPerChannel =
        spd_.ddr5.memoryChannelBusWidth & 0x07;
    if (kPrimaryBusWidthPerChannel > 0x03)
    {
        if (debug)
        {
            std::cerr << "Number of `Primary bus width per channel` is invalid"
                      << std::endl;
        }
        return std::nullopt;
    }
    return 8 * (1 << kPrimaryBusWidthPerChannel);
}

std::optional<uint8_t> SPD::numChannelsPerDimm() const
{
    // DDR5 spec
    // byte 235: [6:5]
    // 00 = 1 channe
    // 01 = 2 channels
    // All others reserved
    const uint8_t kChannelPerDimm =
        (spd_.ddr5.memoryChannelBusWidth >> 5) & 0x03;
    if (kChannelPerDimm > 0x01)
    {
        if (debug)
        {
            std::cerr << "Number of `channels per dimm` is invalid"
                      << std::endl;
        }
        return std::nullopt;
    }
    return kChannelPerDimm + 1;
}

std::optional<uint8_t> SPD::sdramDensityPerDie() const
{
    // DDR5 spec
    // byte 4: [4:0]
    // 00000: No memory; not defined
    // 00001: 4 Gb
    // 00010: 8 Gb
    // 00011: 12 Gb
    // 00100: 16 Gb
    // 00101: 24 Gb
    // 00110: 32 Gb
    // 00111: 48 Gb
    // 01000: 64 Gb
    // All others reserved
    const uint8_t kDensityPerDie = spd_.ddr5.sdram1DensityAndPackage & 0x1F;
    switch (kDensityPerDie)
    {
        case 0x00:
            return 0;
        case 0x01:
            return 4;
        case 0x02:
            return 8;
        case 0x03:
            return 12;
        case 0x04:
            return 16;
        case 0x05:
            return 24;
        case 0x06:
            return 32;
        case 0x07:
            return 48;
        case 0x08:
            return 64;
        default:
            if (debug)
            {
                std::cerr << "Number of `SDRAM density per die` is invalid"
                          << std::endl;
            }
            return std::nullopt;
    }

    // Shouldn't be here.
    return std::nullopt;
}

std::optional<uint8_t> SPD::diePerPackage(bool firstDram) const
{
    // DDR5 spec
    // First DRAM: byte 4
    // Second DRAM: byte 8
    // byte 4/8: [7:5]
    // 000: 1 die; Monolithic SDRAM
    // 001: Reserved
    // 010: 2 die; 2H 3DS
    // 011: 4 die; 4H 3DS
    // 100: 8 die; 8H 3DS
    // 101: 16 die; 16H 3DS
    // All others reserved
    const uint8_t kData = firstDram ? spd_.ddr5.sdram1DensityAndPackage
                                    : spd_.ddr5.sdram2DensityAndPackage;
    const uint8_t kDiePerPackage = (kData >> 5) & 0x07;
    switch (kDiePerPackage)
    {
        case 0x00:
            return 1;
        case 0x02:
            return 2;
        case 0x03:
            return 4;
        case 0x04:
            return 8;
        case 0x05:
            return 16;
        default:
            if (debug)
            {
                std::cerr << "Number of `Die per package` is invalid"
                          << std::endl;
            }
            return std::nullopt;
    }

    // Shouldn't be here.
    return std::nullopt;
}

uint8_t SPD::numPackageRanksPerChannel() const
{
    // DDR5 spec
    // byte 234: [5:3]
    // 000 = 1 Package Rank
    // 001 = 2 Package Ranks
    // 010 = 3 Package Ranks
    // 011 = 4 Package Ranks
    // 100 = 5 Package Ranks
    // 101 = 6 Package Ranks
    // 110 = 7 Package Ranks
    // 111 = 8 Package Ranks
    return ((spd_.ddr5.moduleOrganization >> 3) & 0x07) + 1;
}
