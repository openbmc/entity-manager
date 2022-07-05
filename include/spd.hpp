#pragma once

#include <boost/container/flat_map.hpp>
#include <boost/endian/arithmetic.hpp>
#include <xyz/openbmc_project/Inventory/Item/Dimm/server.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

using Dimm = sdbusplus::xyz::openbmc_project::Inventory::Item::server::Dimm;
using DimmDeviceTypeMap = boost::container::flat_map<uint8_t, Dimm::DeviceType>;
using DimmFormFactorMap = boost::container::flat_map<uint8_t, Dimm::FormFactor>;
using DimmMemoryTechMap = boost::container::flat_map<uint8_t, Dimm::MemoryTech>;
using DimmECCMap = boost::container::flat_map<uint8_t, Dimm::Ecc>;

// Maximal SPD size aross all the generation.
static constexpr size_t kMaxSPDSize = 1024;

struct DDR5Fields
{
    // Block 0~1(0~127): Base Configuration and DRAM Parameters
    // 0 Number of Bytes in SPD Device
    uint8_t bytesTotal;
    // 1 SPD Revision for Base Configuration Parameters
    uint8_t spdRevision;
    // 2 Key Byte / Host Bus Command Protocol Type
    uint8_t hostBusCommandProtocolType;
    // 3 Key Byte / Module Type
    uint8_t moduleType;
    // 4 First SDRAM Density and Package
    uint8_t sdram1DensityAndPackage;
    // 5 First SDRAM Addressing
    uint8_t sdram1Addressing;
    // 6 First SDRAM I/O Width
    uint8_t sdram1IoWidth;
    // 7 First SDRAM Bank Groups & Banks Per Bank Group
    uint8_t sdram1BankGroupAndBanksPerBankGroup;
    // 8 Second SDRAM Density and Package
    uint8_t sdram2DensityAndPackage;
    // 9 Second SDRAM Addressing
    uint8_t sdram2Addressing;
    // 10 Second SDRAM I/O Width
    uint8_t sdram2IoWidth;
    // 11 Second SDRAM Bank Groups & Banks Per Bank Group
    uint8_t sdram2BankGroupAndBanksPerBankGroup;
    // 12 SDRAM Optional Features
    uint8_t sdramOptionalFeatures;
    // 13~15 Reserved. Must be coded as 0x00
    uint8_t reserved13To15[3];
    // 16 SDRAM Nominal Voltage, VDD
    uint8_t vdd;
    // 17 SDRAM Nominal Voltage, VDDQ
    uint8_t vddq;
    // 18 SDRAM Nominal Voltage, VPP
    uint8_t vpp;
    // 19 Reserved. Must be coded as 0x00
    uint8_t reserved19;
    // 20 SDRAM Minimum Cycle Time (tCKAVGmin)
    boost::endian::little_uint16_t t_ckavgmin;
    // 22 SDRAM Maximum Cycle Time (tCKAVGmax)
    boost::endian::little_uint16_t t_ckavgmax;
    // 24 CAS Latencies Supported, 5 bytes in total
    uint8_t cas[5];
    // 29 Reserved. Must be coded as 0x00
    uint8_t reserved29;
    // 30 SDRAM Minimum CAS Latency Time (tAAmin)
    boost::endian::little_uint16_t t_aamin;
    // 32 SDRAM Minimum RAS to CAS Delay Time (tRCDmin)
    boost::endian::little_uint16_t t_rcdmin;
    // 34 SDRAM Minimum Row Precharge Delay Time (tRPmin)
    boost::endian::little_uint16_t t_rpmin;
    // 36 SDRAM Minimum Active to Precharge Delay Time (tRASmin)
    boost::endian::little_uint16_t t_rasmin;
    // 38 SDRAM Minimum Active to Active/Refresh Delay Time (tRCmin)
    boost::endian::little_uint16_t t_rcmin;
    // 40 SDRAM Minimum Write Recovery Time (tWRmin)
    boost::endian::little_uint16_t t_wrmin;
    // 42 SDRAM Minimum Refresh Recovery Delay Time (tRFC1min, tRFC1_slrmin)
    boost::endian::little_uint16_t t_rfc1_slrmin;
    // 44 SDRAM Minimum Refresh Recovery Delay Time (tRFC2min, tRFC2_slrmin)
    boost::endian::little_uint16_t t_rfc2_slrmin;
    // 46 SDRAM Minimum Refresh Recovery Delay Time (tRFCsbmin,
    // tRFCsb_slrmin)
    boost::endian::little_uint16_t t_rfcsb_slrmin;
    // 48 SDRAM Minimum Refresh Recovery Delay Time, 3DS Different Logical
    // Rank (tRFC1_dlr min)
    boost::endian::little_uint16_t t_rfc1_dlrmin;
    // 50 SDRAM Minimum Refresh Recovery Delay Time, 3DS Different Logical
    // Rank (tRFC2_dlr min)
    boost::endian::little_uint16_t t_rfc2_dlrmin;
    // 52 SDRAM Minimum Refresh Recovery Delay Time, 3DS Different Logical
    // Rank (tRFCsb_dlr min)
    boost::endian::little_uint16_t t_rfcsb_dlrmin;
    // 54 SDRAM Refresh Management, First Byte, First SDRAM
    uint8_t sdramRefresh11;
    // 55 SDRAM Refresh Management, Second Byte, First SDRAM
    uint8_t sdramRefresh21;
    // 56 SDRAM Refresh Management, First Byte, Second SDRAM
    uint8_t sdramRefresh12;
    // 57 SDRAM Refresh Management, Second Byte, Second SDRAM
    uint8_t sdramRefresh22;
    // 58~127 Reserved. Must be coded as 0x00
    uint8_t reserved58To127[70];
    // Block 2(128~191): Reserved for future use
    // 128~191 Reserved for future use
    uint8_t reserved128To191[64];
    // Block 3~7(192~511): Standard Module Parameters
    // 192 SPD Revision for bytes 192~447
    uint8_t spdRevision192To447;
    // 193 Reserved. Must be coded as 0x00
    uint8_t reserved193;
    // 194 SPD Manufacturer's ID Code
    boost::endian::little_uint16_t spdManufacturerId;
    // 196 SPD Device Type
    uint8_t spdDeviceType;
    // 197 SPD Device Revision Number
    uint8_t spdDeviceRevisionNumber;
    // 198 PMIC 0 Manufacturer's ID Code
    boost::endian::little_uint16_t pmic0ManufacturerId;
    // 200 PMIC 0 Device Type
    uint8_t pmic0DeviceType;
    // 201 PMIC 0 Device Revision Number
    uint8_t pmic0DeviceRevisionNumber;
    // 202 PMIC 1 Manufacturer's ID Code
    boost::endian::little_uint16_t pmic1ManufacturerId;
    // 204 PMIC 1 Device Type
    uint8_t pmic1DeviceType;
    // 205 PMIC 1 Device Revision Number
    uint8_t pmic1DeviceRevisionNumber;
    // 206 PMIC 2 Manufacturer's ID Code
    boost::endian::little_uint16_t pmic2ManufacturerId;
    // 208 PMIC 2 Device Type
    uint8_t pmic2DeviceType;
    // 209 PMIC 2 Device Revision Number
    uint8_t pmic2DeviceRevisionNumber;
    // 210 Thermal Sensor Manufacturer's ID Code
    boost::endian::little_uint16_t thermalSensorManufacturerId;
    // 212 Thermal Sensor Device Type
    uint8_t thermalSensorDeviceType;
    // 213 Thermal Sensor Device Revision Number
    uint8_t thermalSensorDeviceRevisionNumber;
    // 214~229 Reserved
    uint8_t reserved214To229[16];
    // 230 Module Nominal Height
    uint8_t moduleNominalHeight;
    // 231 Module Maximum Thickness
    uint8_t moduleMaximumThickness;
    // 232 Reference Raw Card Used
    uint8_t referenceRawCardUsed;
    // 233 DIMM Attributes
    uint8_t dimmAttributes;
    // 234 Module Organization
    uint8_t moduleOrganization;
    // 235 Memory Channel Bus Width
    uint8_t memoryChannelBusWidth;
    // 236~239 Reserved
    uint8_t reserved236To239[4];
    // 240~447 Module Type Specific Information
    uint8_t moduleTypeSpecificInformation[208];
    // 448~509 Reserved for future use
    uint8_t reserved448To509[62];
    // 510~511 CRC for SPD bytes 0~509
    boost::endian::little_uint16_t crc;
    // Block 8~9(512~639): Manufacturing Information
    // 512 Module Manufacturer’s ID Code
    boost::endian::little_uint16_t moduleManufacturerId;
    // 514 Module Manufacturing Location
    uint8_t moduleManufacturingLocation;
    // 515~516 Module Manufacturing Date
    uint8_t moduleManufacturingDate[2];
    // 517~520 Module Serial Number
    uint8_t serialNumber[4];
    // 521~550 Module Part Number
    uint8_t partNumber[30];
    // 551 Module Revision Code
    uint8_t moduleRevisionCode;
    // 552 DRAM Manufacturer’s ID Code
    boost::endian::little_uint16_t dramManufacturerId;
    // 554 DRAM Stepping
    uint8_t dramStepping;
    // 555~637 Manufacturer’s Specific Data
    uint8_t manufacturerData[83];
    // 638~639 Reserved. Must be coded as 0x00
    uint8_t reserved638To639[2];
    // Block 10~15(640~1023): End User Programmable
    // 640~1023 End User Programmable
    uint8_t reserved640To1023[384];
};

// DDR5 spec
// byte 2: [7:0]
// 0x00 Reserved
// 0x01 Fast Page Mode
// 0x02 EDO
// 0x03 Pipelined Nibble
// 0x04 SDRAM
// 0x05 ROM
// 0x06 DDR SGRAM
// 0x07 DDR SDRAM
// 0x08 DDR2 SDRAM
// 0x09 DDR2 SDRAM FB-DIMM
// 0x0A DDR2 SDRAM FB-DIMM PROBE
// 0x0B DDR3 SDRAM
// 0x0C DDR4 SDRAM
// 0x0D Reserved
// 0x0E DDR4E SDRAM
// 0x0F LPDDR3 SDRAM
// 0x10 LPDDR4 SDRAM
// 0x11 LPDDR4X SDRAM
// 0x12 DDR5 SDRAM
// 0x13 LPDDR5 SDRAM
// 0x14 DDR5 NVDIMM-P
const static DimmDeviceTypeMap deviceTypeMap = {
    {0x01, Dimm::DeviceType::FastPageMode},
    {0x02, Dimm::DeviceType::EDO},
    {0x03, Dimm::DeviceType::PipelinedNibble},
    {0x04, Dimm::DeviceType::SDRAM},
    {0x05, Dimm::DeviceType::ROM},
    {0x06, Dimm::DeviceType::DDR_SGRAM},
    {0x07, Dimm::DeviceType::DDR},
    {0x08, Dimm::DeviceType::DDR2},
    {0x09, Dimm::DeviceType::DDR2_SDRAM_FB_DIMM},
    {0x0A, Dimm::DeviceType::DDR2_SDRAM_FB_DIMM_PROB},
    {0x0B, Dimm::DeviceType::DDR3},
    {0x0C, Dimm::DeviceType::DDR4},
    {0x0E, Dimm::DeviceType::DDR4E_SDRAM},
    {0x0F, Dimm::DeviceType::LPDDR3_SDRAM},
    {0x10, Dimm::DeviceType::LPDDR4_SDRAM},
    {0x12, Dimm::DeviceType::DDR5},
    {0x13, Dimm::DeviceType::LPDDR5_SDRAM},
    {0x14, Dimm::DeviceType::Other},
};

// DDR5 spec
// byte 3: [3:0]
// 0000: Reserved
// 0001: RDIMM
// 0010: UDIMM
// 0011: SODIMM
// 0100: LRDIMM
// 0101: Reserved
// 0110: Reserved
// 0111: Reserved
// 1000: Reserved
// 1001: Reserved
// 1010: DDIMM
// 1011: Solder down
// 1100: Reserved
// 1101: Reserved
// 1110: Reserved
// 1111: Reserved
const static DimmFormFactorMap formFactorMap = {
    {0x01, Dimm::FormFactor::RDIMM},
    {0x02, Dimm::FormFactor::UDIMM},
    {0x03, Dimm::FormFactor::SO_DIMM},
    {0x04, Dimm::FormFactor::LRDIMM},
};

// DDR5 spec
// byte 3: [7]          [6:4]
// 0: Not hybrid        000: Not hybrid
// 1: Hybrid module     001: NVDIMM-N Hybrid
//                      010: NVDIMM-P Hybrid
const static DimmMemoryTechMap memoryTechMap = {
    {0x00, Dimm::MemoryTech::Other},
    {0x90, Dimm::MemoryTech::NVDIMM_N},
    {0xA0, Dimm::MemoryTech::NVDIMM_P},
};

// DDR5 spec
// byte 235: [4:3]
// 00 = 0 bits
// 01 = 4 bits
// 10 = 8 bits
const static DimmECCMap eccMap = {
    {0x00, Dimm::Ecc::NoECC},
    {0x08, Dimm::Ecc::MultiBitECC},
    {0x10, Dimm::Ecc::MultiBitECC},
};

struct SPDKeyFields
{
    // First 4 bytes are the key bytes that has to be existed across all the
    // generation. 0 Number of Bytes in SPD Device
    uint8_t spdSize;
    // 1 SPD Revision for Base Configuration Parameters
    uint8_t spdRevision;
    // 2 DRAM device Type
    // The name of this byte is changed to "Host Bus Command Protocol Type" in
    // DDR5. Usage is the same.
    uint8_t deviceType;
    // 3 Module Type
    uint8_t moduleType;
};

union SPDContent
{
    DDR5Fields ddr5;
    uint8_t raw[kMaxSPDSize];
    static_assert(sizeof(DDR5Fields) <= kMaxSPDSize, "DDR5Fields size error");
};

class SPD
{
  public:
    SPD() = delete;
    ~SPD() = default;

    // Minimal required SPD size
    static constexpr size_t kMinRequiredSPDSize = sizeof(SPDKeyFields);

    // Parse SPD instance with content
    static std::unique_ptr<SPD> parseSPD(std::span<const uint8_t> spdImage);

    // Check if input type is supported
    static bool isSupportedType(Dimm::DeviceType type);

    // Calculate the crc16 for the SPD content. The crc16 algorithm is defined
    // in JEDEC
    static uint16_t calcJedecCRC16(std::span<const uint8_t> data);

    // Returns the total size of the serial memory used to hold the SPD data or
    // nullopt is the total size is unrecognized
    static std::optional<uint32_t> spdSize(uint8_t byte);

    // Type of this device
    static Dimm::DeviceType deviceType(uint8_t byte);

    // Module type of this dimm
    static std::optional<Dimm::FormFactor> moduleType(uint8_t byte);

    // None static calcJedecCRC16
    uint16_t calcJedecCRC16() const;

    // None static spdSize
    std::optional<uint32_t> spdSize() const;

    // None static deviceType
    Dimm::DeviceType deviceType() const;

    // None static moduleType
    std::optional<Dimm::FormFactor> moduleType() const;

    // DIMM size in kilobyte
    std::optional<uint64_t> dimmSizeKB() const;

    // Data width
    std::optional<uint8_t> width() const;

    // List of supported speeds for the DIMM in MHz.
    std::vector<uint16_t> speeds() const;

    // The maximum capable clock speed in megahertz
    std::optional<uint16_t> maxClockSpeed() const;

    // Number of groups
    std::optional<uint8_t> groupNum() const;

    // Error-Correcting Code
    Dimm::Ecc ecc() const;

    // CAS Latency (CL) values are supported
    std::vector<uint16_t> casLatencies() const;

    // Revision code
    uint8_t revisionCode() const;

    // Technology of this DIMM
    Dimm::MemoryTech tech() const;

    // Manufacturer ID output can be interpreted according to JEDEC's
    // manufacturer ID list (JEP106)
    std::optional<std::string> manufacturerId() const;

    // Location code defined by manufacturer
    uint8_t manufacturerLocation() const;

    // In the SPD spec, the manufacturingDate is composed by 2 bytes.
    // 1 byte for years and 1 byte for "week in the year"
    // We convert that into std::chrono::time_point by useing the beginning of
    // that year + std::chrono::weeks(week) e.g. year=2022, week=4 will be
    // converted to chrono::time_point "01/29/2022 00:00:00"
    std::optional<std::chrono::time_point<std::chrono::system_clock>>
        manufacturingDate() const;

    // Convert the hex serial number to string
    std::string serialNumber() const;

    // Returns the part number of this SPD in ASCII format.
    // Unused digits are coded as ASCII blanks (0x20).
    std::string partNumber() const;

    // Stored (not computed) checksum
    uint16_t checksum() const;

    // Returns a span of the raw bytes of this SPD
    std::span<const uint8_t> raw() const;

  private:
    SPD(std::span<const uint8_t> spdImage);

    // These private functions are used to calculate dimm size.
    bool symmetrical() const;
    std::optional<uint8_t> primaryBusWidthPerChannel() const;
    std::optional<uint8_t> numChannelsPerDimm() const;
    std::optional<uint8_t> sdramDensityPerDie() const;
    std::optional<uint8_t> diePerPackage(bool firstDram) const;
    uint8_t numPackageRanksPerChannel() const;

    SPDContent spd_;
};
