#pragma once

#include "spd/spd.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

// JESD400-5 DDR5 Serial Presence Detect (SPD) Contents 0.985, Feb-2021
class DDR5SPD : public SPD
{
  public:
    // Parses SPD information from the given source and returns a new SPD object
    // owned by the caller. Returns NULL on error or if the given SPD is not a
    // DDR5 SPD.
    static std::unique_ptr<DDR5SPD>
        getFromImage(std::span<const uint8_t> spdBytes);

    // Gets a list of supported clock speeds for the DIMM in MHz.
    std::vector<int> speeds() const;

    // Returns DRAM manufacturer ID information. This output can be interpreted
    // according to JEDEC's manufacturer ID list (JEP106).
    std::pair<int8_t, int8_t> dramManufacturerId() const;

    // Returns the SDRAM Density Per Die, in unit of Gb, or -1 on error.
    int8_t sdramDensityPerDie() const;

    // Returns whether the assembly has the same SDRAM density on all ranks or
    // has different SDRAM densities on even and odd ranks.
    bool symmetrical() const;

    int8_t numPackageRanksPerChannel() const;

    // Returns the number of channels per DIMM.
    int8_t numChannelsPerDimm() const;

    // Returns the primary bus width per channel.
    int8_t primaryBusWidthPerChannel() const;

    // Get number of Die in a package
    // byte 4 for the first dram, byte 8 for the second.
    int8_t diePerPackage(bool firstDram) const;

    // Implemented virtual methods from SPD.
    SPDType type() const override;
    std::span<const uint8_t> raw() const override;
    int32_t spdTotalSize() const override;
    int32_t spdUsedSize() const override;
    std::pair<int8_t, int8_t> manufacturerId() const override;
    int8_t manufacturerLocation() const override;
    SPD::Date manufacturingDate() const override;
    ModuleType module() const override;
    std::vector<uint8_t> serialNumber() const override;
    std::string partNumber() const override;
    int8_t revisionCode() const override;
    int64_t dimmSize() const override; // in MiB
    int8_t eccBits() const override;
    int32_t banks() const override;
    int32_t rows() const override;
    int32_t columns() const override;
    int32_t ranks() const override;
    int32_t width() const override;
    uint16_t checksum() const override;

    // Layout of fields in a DDR5 SPD.
    // Individual field descriptions are taken directly from the JEDEC spec.
    // The bytes are in little endian order.
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
        uint16_t t_ckavgmin;
        // 22 SDRAM Maximum Cycle Time (tCKAVGmax)
        uint16_t t_ckavgmax;
        // 24 CAS Latencies Supported, First Byte
        uint8_t cas1;
        // 25 CAS Latencies Supported, Second Byte
        uint8_t cas2;
        // 26 CAS Latencies Supported, Third Byte
        uint8_t cas3;
        // 27 CAS Latencies Supported, Fourth Byte
        uint8_t cas4;
        // 28 CAS Latencies Supported, Fifth Byte
        uint8_t cas5;
        // 29 Reserved. Must be coded as 0x00
        uint8_t reserved29;
        // 30 SDRAM Minimum CAS Latency Time (tAAmin)
        uint16_t t_aamin;
        // 32 SDRAM Minimum RAS to CAS Delay Time (tRCDmin)
        uint16_t t_rcdmin;
        // 34 SDRAM Minimum Row Precharge Delay Time (tRPmin)
        uint16_t t_rpmin;
        // 36 SDRAM Minimum Active to Precharge Delay Time (tRASmin)
        uint16_t t_rasmin;
        // 38 SDRAM Minimum Active to Active/Refresh Delay Time (tRCmin)
        uint16_t t_rcmin;
        // 40 SDRAM Minimum Write Recovery Time (tWRmin)
        uint16_t t_wrmin;
        // 42 SDRAM Minimum Refresh Recovery Delay Time (tRFC1min, tRFC1_slrmin)
        uint16_t t_rfc1_slrmin;
        // 44 SDRAM Minimum Refresh Recovery Delay Time (tRFC2min, tRFC2_slrmin)
        uint16_t t_rfc2_slrmin;
        // 46 SDRAM Minimum Refresh Recovery Delay Time (tRFCsbmin,
        // tRFCsb_slrmin)
        uint16_t t_rfcsb_slrmin;
        // 48 SDRAM Minimum Refresh Recovery Delay Time, 3DS Different Logical
        // Rank (tRFC1_dlr min)
        uint16_t t_rfc1_dlrmin;
        // 50 SDRAM Minimum Refresh Recovery Delay Time, 3DS Different Logical
        // Rank (tRFC2_dlr min)
        uint16_t t_rfc2_dlrmin;
        // 52 SDRAM Minimum Refresh Recovery Delay Time, 3DS Different Logical
        // Rank (tRFCsb_dlr min)
        uint16_t t_rfcsb_dlrmin;
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
        // 194 SPD Manufacturer's ID Code, First Byte
        uint8_t spdManufacturerId1;
        // 195 SPD Manufacturer's ID Code, Second Byte
        uint8_t spdManufacturerId2;
        // 196 SPD Device Type
        uint8_t spdDeviceType;
        // 197 SPD Device Revision Number
        uint8_t spdDeviceRevisionNumber;
        // 198 PMIC 0 Manufacturer's ID Code, First Byte
        uint8_t pmic0ManufacturerId1;
        // 199 PMIC 0 Manufacturer's ID Code, Second Byte
        uint8_t pmic0ManufacturerId2;
        // 200 PMIC 0 Device Type
        uint8_t pmic0DeviceType;
        // 201 PMIC 0 Device Revision Number
        uint8_t pmic0DeviceRevisionNumber;
        // 202 PMIC 1 Manufacturer's ID Code, First Byte
        uint8_t pmic1ManufacturerId1;
        // 203 PMIC 1 Manufacturer's ID Code, Second Byte
        uint8_t pmic1ManufacturerId2;
        // 204 PMIC 1 Device Type
        uint8_t pmic1DeviceType;
        // 205 PMIC 1 Device Revision Number
        uint8_t pmic1DeviceRevisionNumber;
        // 206 PMIC 2 Manufacturer's ID Code, First Byte
        uint8_t pmic2ManufacturerId1;
        // 207 PMIC 2 Manufacturer's ID Code, Second Byte
        uint8_t pmic2ManufacturerId2;
        // 208 PMIC 2 Device Type
        uint8_t pmic2DeviceType;
        // 209 PMIC 2 Device Revision Number
        uint8_t pmic2DeviceRevisionNumber;
        // 210 Thermal Sensor Manufacturer's ID Code, First Byte
        uint8_t thermalSensorManufacturerId1;
        // 211 Thermal Sensor Manufacturer's ID Code, Second Byte
        uint8_t thermalSensorManufacturerId2;
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
        uint8_t crc[2];
        // Block 8~9(512~639): Manufacturing Information
        // 512 Module Manufacturer’s ID Code, First Byte
        uint8_t moduleManufacturerId1;
        // 513 Module Manufacturer’s ID Code, Second Byte
        uint8_t moduleManufacturerId2;
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
        // 552 DRAM Manufacturer’s ID Code, First Byte
        uint8_t dramManufacturerId1;
        // 553 DRAM Manufacturer’s ID Code, Second Byte
        uint8_t dramManufacturerId2;
        // 554 DRAM Stepping
        uint8_t dramStepping;
        // 555~637 Manufacturer’s Specific Data
        uint8_t manufacturerData[83];
        // 638~639 Reserved. Must be coded as 0x00
        uint8_t reserved638To639[2];
        // Block 10~15(640~1023): End User Programmable
        // 640~1023 End User Programmable
        uint8_t reserved640To1023[384];
    } __attribute__((packed));

  private:
    // This should only be constructed by the GetFromImage factory function.
    explicit DDR5SPD(std::span<const uint8_t> spdBytes);
    std::vector<uint8_t> spdBytes_;
    const DDR5Fields* spd_;
};
