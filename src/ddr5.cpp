#include "spd/ddr5.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

std::unique_ptr<DDR5SPD>
    DDR5SPD::getFromImage(std::span<const uint8_t> spdBytes)
{
    // Check type
    if (getSPDType(spdBytes) != SPDType::spdTypeDDR5)
    {
        std::cerr
            << "Cannot parse DDR5 SPD: given SPD image is not a DDR5 SPD.\n";
        return nullptr;
    }

    // Check size
    const auto kSize = getSPDSize(spdBytes);
    if (kSize == -1 || static_cast<uint32_t>(kSize) != spdBytes.size())
    {
        std::cerr << "Cannot parse DDR5 SPD: given SPD size is incorrect.\n";
        return nullptr;
    }
    return std::unique_ptr<DDR5SPD>(new DDR5SPD(spdBytes));
}

std::vector<int> DDR5SPD::speeds() const
{
    const std::vector<int> possibleMhz = {4800, 7200};
    std::vector<int> results;

    int32_t maxSpeed = std::floor(2000000.0 / spd_->t_ckavgmin); // MHz
    int32_t minSpeed = std::ceil(2000000.0 / spd_->t_ckavgmax);  // MHz

    for (auto s : possibleMhz)
    {
        if (s <= maxSpeed && s >= minSpeed)
        {
            results.emplace_back(s);
        }
    }

    return results;
}

std::pair<int8_t, int8_t> DDR5SPD::dramManufacturerId() const
{
    if (spdUsedSize() < 1024)
    {
        // Return with negative values to indicate error.
        std::cerr << "SPD is not contained DramManufacturerId\n";
        return std::make_pair<int8_t, int8_t>(-1, -1);
    }
    return std::make_pair<int8_t, int8_t>(spd_->dramManufacturerId1 & 0x7F,
                                          spd_->dramManufacturerId2 & 0x7F);
}

int8_t DDR5SPD::sdramDensityPerDie() const
{
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
    const uint8_t kDensityPerDie = spd_->sdram1DensityAndPackage & 0x1f;
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
            // Return with negative values to indicate error.
            std::cerr << "Number of `SDRAM density per die` is invalid\n";
            return -1;
    }
}

bool DDR5SPD::symmetrical() const
{
    // byte 234: [6]
    // 0: Symmetrical
    // 1: Asymmetrical
    return (spd_->moduleOrganization & 0x40) == 0;
}

int8_t DDR5SPD::numPackageRanksPerChannel() const
{
    // byte 234: [5:3]
    // 000 = 1 Package Rank
    // 001 = 2 Package Ranks
    // 010 = 3 Package Ranks
    // 011 = 4 Package Ranks
    // 100 = 5 Package Ranks
    // 101 = 6 Package Ranks
    // 110 = 7 Package Ranks
    // 111 = 8 Package Ranks
    return ((spd_->moduleOrganization >> 3) & 0x07) + 1;
}

int8_t DDR5SPD::numChannelsPerDimm() const
{
    // byte 235: [6:5]
    // 00 = 1 channe
    // 01 = 2 channels
    // All others reserved
    const int8_t kChannelPerDimm = (spd_->memoryChannelBusWidth >> 5) & 0x03;
    if (kChannelPerDimm > 0x01)
    {
        // Return with negative values to indicate error.
        std::cerr << "Number of `channels per dimm` is invalid\n";
        return -1;
    }
    return kChannelPerDimm + 1;
}

int8_t DDR5SPD::primaryBusWidthPerChannel() const
{
    // byte 235 [2:0]
    // 000 = 8 bits
    // 001 = 16 bits
    // 010 = 32 bits
    // 011 = 64 bits
    // All others reserved
    const int8_t kPrimaryBusWidthPerChannel =
        spd_->memoryChannelBusWidth & 0x07;
    switch (kPrimaryBusWidthPerChannel)
    {
        case 0x00:
            return 8;
        case 0x01:
            return 16;
        case 0x02:
            return 32;
        case 0x03:
            return 64;
        default:
            // Return with negative values to indicate error.
            std::cerr
                << "Number of `Primary bus width per channel` is invalid\n";
            return -1;
    }
}

int8_t DDR5SPD::diePerPackage(bool firstDram) const
{
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
    const uint8_t kData = firstDram ? spd_->sdram1DensityAndPackage
                                    : spd_->sdram2DensityAndPackage;
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
            // Return with negative values to indicate error.
            std::cerr << "Number of `Die per package` is invalid\n";
            return -1;
    }
}

SPD::SPDType DDR5SPD::type() const
{
    return static_cast<SPDType>(spd_->hostBusCommandProtocolType);
}

std::span<const uint8_t> DDR5SPD::raw() const
{
    return std::span<const uint8_t>{spdBytes_};
}

int32_t DDR5SPD::spdTotalSize() const
{
    // byte 0: [6:4]
    // 000: Undefined
    // 001: 256
    // 010: 512
    // 011: 1024
    // All others reserved
    const uint8_t kByteTotal = (spd_->bytesTotal >> 4) & 0x07;
    switch (kByteTotal)
    {
        case 0x01:
            return 256;
        case 0x02:
            return 512;
        case 0x03:
            return 1024;
        default:
            // Return with negative values to indicate error.
            std::cerr << "SPD size is invalid\n";
            return -1;
    }
}

// On prior generations, spdUsedSize() and spdTotalSize() were referring to two
// distinct sizes, but on DDR5 there is no distinction.
int32_t DDR5SPD::spdUsedSize() const
{
    return spdTotalSize();
}

std::pair<int8_t, int8_t> DDR5SPD::manufacturerId() const
{
    if (spdUsedSize() < 1024)
    {
        // Return with negative values to indicate error.
        std::cerr << "SPD is not contained ModuleManufacturerId\n";
        return std::make_pair<int8_t, int8_t>(-1, -1);
    }
    return std::make_pair<int8_t, int8_t>(spd_->moduleManufacturerId1 & 0x7F,
                                          spd_->moduleManufacturerId2 & 0x7F);
}

int8_t DDR5SPD::manufacturerLocation() const
{
    if (spdUsedSize() < 1024)
    {
        // Return with negative values to indicate error.
        std::cerr << "SPD is not contained ModuleManufacturerId\n";
        return -1;
    }

    return spd_->moduleManufacturingLocation;
}

SPD::Date DDR5SPD::manufacturingDate() const
{
    Date date = {-1, -1};
    if (spdUsedSize() < 1024)
    {
        // Return with negative values to indicate error.
        std::cerr << "SPD is not contained ManufacturingDate\n";
        return date;
    }

    date.year =
        static_cast<int32_t>((spd_->moduleManufacturingDate[0] >> 4) * 10 +
                             (spd_->moduleManufacturingDate[0] & 0x0F)) +
        2000;
    date.week =
        static_cast<int32_t>((spd_->moduleManufacturingDate[1] >> 4) * 10 +
                             (spd_->moduleManufacturingDate[1] & 0x0F));
    return date;
}

SPD::ModuleType DDR5SPD::module() const
{
    return static_cast<ModuleType>(spd_->moduleType);
}

std::vector<uint8_t> DDR5SPD::serialNumber() const
{
    if (spdUsedSize() < 1024)
    {
        // Return with negative values to indicate error.
        std::cerr << "SPD is not contained SerialNumber\n";
        return {};
    }
    return std::vector<uint8_t>{
        spd_->serialNumber, spd_->serialNumber + sizeof(spd_->serialNumber)};
}

std::string DDR5SPD::partNumber() const
{
    if (spdUsedSize() < 1024)
    {
        // Return with negative values to indicate error.
        std::cerr << "SPD is not contained PartNumber\n";
        return "";
    }

    return std::string{spd_->partNumber,
                       spd_->partNumber + sizeof(spd_->partNumber)};
}

int8_t DDR5SPD::revisionCode() const
{
    if (spdUsedSize() < 1024)
    {
        // Return with negative values to indicate error.
        std::cerr << "SPD is not contained ModuleRevisionCode\n";
        return -1;
    }
    return spd_->moduleRevisionCode;
}

int64_t DDR5SPD::dimmSize() const
{
    const auto kNumChannelsPerDimm = numChannelsPerDimm();
    const auto kPrimaryBusWidthPerChannel = primaryBusWidthPerChannel();
    const auto kWidth = width();
    const auto kDiePerPackage1 = diePerPackage(true);
    const auto kDiePerPackage2 = diePerPackage(false);
    const auto kSdramDensityPerDie = sdramDensityPerDie();
    const auto kNumPackageRanksPerChannel = numPackageRanksPerChannel();
    if (kNumChannelsPerDimm == -1 || kPrimaryBusWidthPerChannel == -1 ||
        kWidth == -1 || kDiePerPackage1 == -1 || kDiePerPackage2 == -1 ||
        kSdramDensityPerDie == -1 || kNumPackageRanksPerChannel == -1)
    {
        std::cerr << "Can't calculate dimmSize\n";
        return -1;
    }

    if (symmetrical())
    {
        // To calculate the total capacity in bytes for a symmetric module, the
        // following math applies: Capacity in bytes = Number of channels per
        // DIMM * Primary bus width per channel / SDRAM I/O Width * Die per
        // package * SDRAM density per die / 8 * Package ranks per channel

        // The calculation result is in unit of GiB, but the output is expected
        // to be in unit of MiB. So left shift 10 bits.
        return (static_cast<int64_t>(kNumChannelsPerDimm) *
                kPrimaryBusWidthPerChannel / kWidth * kDiePerPackage1 *
                kSdramDensityPerDie / 8 * kNumPackageRanksPerChannel)
               << 10;
    }
    // To calculate the total capacity in bytes for an asymmetric module,
    // the following math applies: Capacity in bytes = Capacity of even
    // ranks (first SDRAM type) + Capacity of odd ranks (second SDRAM type)

    // The calculation result is in unit of GiB, but the output is expected
    // to be in unit of MiB. So left shift 10 bits.
    return (static_cast<int64_t>(kNumChannelsPerDimm) *
            kPrimaryBusWidthPerChannel / kWidth *
            (kDiePerPackage1 + kDiePerPackage2) * kSdramDensityPerDie / 8)
           << 10;
}

int8_t DDR5SPD::eccBits() const
{
    // byte 235: [4:3]
    // 00: 0 bits (no extension)
    // 01: 4 bits
    // 10: 8 bits
    // All others reserved
    const int8_t kECCBits = (spd_->memoryChannelBusWidth >> 3) & 0x03;
    if (kECCBits > 0x02)
    {
        // Return with negative values to indicate error.
        std::cerr << "Number of `ecc bits` is invalid\n";
        return -1;
    }
    return 4 * kECCBits;
}

int32_t DDR5SPD::banks() const
{
    // byte 7:  [7:5]                   [2:0]
    //          000: 1 bank group       000: 1 bank per bank group
    //          001: 2 bank groups      001: 2 bank per bank group
    //          010: 4 bank groups      010: 4 bank per bank group
    //          011: 8 bank groups      All others reserved
    //          All others reserved
    const int32_t kBankPerGroup =
        spd_->sdram1BankGroupAndBanksPerBankGroup & 0x07;
    const int32_t kBankGroups =
        (spd_->sdram1BankGroupAndBanksPerBankGroup >> 5) & 0x07;
    if (kBankPerGroup > 0x02 || kBankGroups > 0x03)
    {
        // Return with negative values to indicate error.
        std::cerr << "Number of `banks` is invalid\n";
        return -1;
    }

    return 1 << (kBankPerGroup + kBankGroups);
}

int32_t DDR5SPD::rows() const
{
    // byte 5: [4:0]
    // 00000: 16 rows
    // 00001: 17 rows
    // 00010: 18 rows
    // All others reserved
    const auto kRows = spd_->sdram1Addressing & 0x1F;
    if (kRows > 0x02)
    {
        // Return with negative values to indicate error.
        std::cerr << "Number of `rows` is invalid\n";
        return -1;
    }

    return 1 << (kRows + 16);
}

int32_t DDR5SPD::columns() const
{
    // byte 5: [7:5]
    // 000: 10 columns
    // 001: 11 columns
    // All others reserved
    const auto kCols = (spd_->sdram1Addressing >> 5) & 0x07;
    if (kCols > 0x01)
    {
        // Return with negative values to indicate error.
        std::cerr << "Number of `columns` is invalid\n";
        return -1;
    }

    return 1 << (kCols + 10);
}

int32_t DDR5SPD::ranks() const
{
    const auto kDiePerPackage1 = diePerPackage(true);
    const auto kDiePerPackage2 = diePerPackage(false);
    const auto kNumPackageRanksPerChannel = numPackageRanksPerChannel();
    if (kDiePerPackage1 == -1 || kDiePerPackage2 == -1 ||
        kNumPackageRanksPerChannel == -1)
    {
        std::cerr << "Can't calculate ranks\n";
        return -1;
    }

    if (symmetrical())
    {
        return kDiePerPackage1 * kNumPackageRanksPerChannel;
    }
    // Asymmetrical
    return kDiePerPackage1 + kDiePerPackage2;
}

int32_t DDR5SPD::width() const
{
    // byte 6: [7:5]
    // 000: x4
    // 001: x8
    // 010: x16
    // 011: x32
    // All others reserved
    const int8_t kWidth = (spd_->sdram1IoWidth >> 5) & 0x07;
    if (kWidth > 0x03)
    {
        // Return with negative values to indicate error.
        std::cerr << "Number of `IO width` is invalid\n";
        return -1;
    }

    return 4 * (1 << kWidth);
}

uint16_t DDR5SPD::checksum() const
{
    if (spdUsedSize() < 512)
    {
        return 0;
    }
    return (spd_->crc[1] << 8) | spd_->crc[0];
}

DDR5SPD::DDR5SPD(std::span<const uint8_t> spdBytes) :
    spdBytes_(spdBytes.begin(), spdBytes.end())
{
    spd_ = reinterpret_cast<DDR5Fields*>(spdBytes_.data());
}
