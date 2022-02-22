#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <array>
#include <optional>
#include <vector>

enum class DRAMType : uint8_t
{
    dramTypeUnknown = 0x00,
    dramTypeDDR1 = 0x07,
    dramTypeDDR2 = 0x08,
    dramTypeFBDIMM = 0x09,
    dramTypeDDR3 = 0x0B,
    dramTypeDDR4 = 0x0C,
    dramTypeDDR5 = 0x12,
};

enum class ModuleType : uint8_t
{
    moduleTypeUnknown = 0x00,
    moduleTypeRDIMM = 0x01,
    moduleTypeUDIMM = 0x02,
    moduleTypeSODIMM = 0x03,
    moduleTypeLRDIMM = 0x04,
    moduleTypeDDIMM = 0x0A,
    moduleTypeSolderDown = 0x0B,
    moduleTypeNVNRDIMM = 0x91,
    moduleTypeNVPRDIMM = 0x92,
};

struct MFRIDJEP106
{
    uint8_t bankNumber;
    uint8_t OffsetInBank;
};

class SPD
{
  private:
    struct SPDKeyFields
    {
        // First 4 bytes are the key bytes that has to be existed across all the
        // generation. 0 Number of Bytes in SPD Device
        uint8_t bytesTotal;
        // 1 SPD Revision for Base Configuration Parameters
        uint8_t spdRevision;
        // 2 DRAM device Type
        // The name of this byte is changed to "Host Bus Command Protocol Type" in
        // DDR5. Usage is the same.
        uint8_t dramType;
        // 3 Module Type
        uint8_t moduleType;
    };

  public:
    virtual ~SPD() = default;

    // Minimal required SPD size.
    static constexpr size_t kMinRequiredSPDSize = sizeof(SPDKeyFields);

    // Returns the type of the DRAM, or DRAMType::dramTypeUnknown on error.
    static DRAMType getDRAMType(std::span<const uint8_t> spdImage);

    // Returns the total size of SPD, or nullopt on error.
    static std::optional<uint32_t> getSPDSize(std::span<const uint8_t> spdImage);

    // Parses SPD information from the given source and returns a new SPD object
    // owned by the caller. Returns nullptr on error or if the type of the given
    // SPD is unsupported.
    static std::unique_ptr<SPD> getFromImage(std::span<const uint8_t> spdImage);

    // Calculate the crc16 for the SPD content. The crc16 algorithm is defined
    // in JEDEC
    static uint16_t calcJedecCRC16(std::span<const uint8_t> spdImage);

    // Convert DRAM type to string
    static std::string dramTypeToStr(DRAMType type);
    // Convert a byte to DRAMType
    static DRAMType byteToDRAMType(uint8_t b);
    // Convert a byte to ModuleType
    static ModuleType byteToModuleType(uint8_t b);

    // Returns the type of this DRAM, or dramTypeUnknown if the DRAM type is
    // unknown.
    virtual DRAMType type() const = 0;

    // Returns a copy of the raw bytes of this SPD.
    virtual std::span<const uint8_t> raw() const = 0;

    // Returns the number of bytes used by the module manufacturer in this SPD or nullopt is the used size is unrecognized.
    virtual std::optional<uint32_t> spdUsedSize() const = 0;

    // Returns the total size of the serial memory used to hold the SPD data or nullopt is the used size is unrecognized.
    virtual std::optional<uint32_t> spdTotalSize() const = 0;

    // Gets a list of supported clock speeds for the DIMM in MHz.
    virtual std::vector<uint16_t> speeds() const = 0;

    // Manufacturer ID output can be interpreted according to JEDEC's
    // manufacturer ID list (JEP106).
    virtual std::optional<MFRIDJEP106> manufacturerId() const = 0;

    virtual uint8_t manufacturerLocation() const = 0;

    // In the SPD spec, the manufacturingDate is composed by 2 bytes.
    // 1 byte for years and 1 byte for "week in the year"
    // We convert that into std::chrono::time_point by useing the beginning of
    // that year + std::chrono::weeks(week) e.g. year=2022, week=4 will be
    // converted to chrono::time_point "01/29/2022 00:00:00"
    virtual std::optional<std::chrono::time_point<std::chrono::system_clock>>
        manufacturingDate() const = 0;

    virtual ModuleType module() const = 0;

    // The supplier may use whatever decode method desired to maintain a unique serial number.
    virtual std::array<uint8_t, 4> serialNumber() const = 0;

    // Returns the part number of this SPD in ASCII format.
    // Unused digits are coded as ASCII blanks (0x20).
    virtual std::string partNumber() const = 0;

    virtual uint8_t revisionCode() const = 0;

    virtual std::optional<uint64_t> dimmSizeMiB() const = 0;

    virtual std::optional<uint8_t> eccBits() const = 0;

    virtual std::optional<uint32_t> banks() const = 0;

    virtual std::optional<uint32_t> rows() const = 0;

    virtual std::optional<uint32_t> columns() const = 0;

    // Returns the total number of logical ranks of the DIMM.
    virtual std::optional<uint32_t> ranks() const = 0;

    virtual std::optional<uint32_t> width() const = 0;

    // Stored (not computed) checksum.
    virtual uint16_t checksum() const = 0; 

    // Recalculate checksum then compare with stored checksum
    virtual bool checkChecksum() const = 0; 
};
