#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <ctime>
#include <chrono>

enum class SPDType: uint8_t
{
    spdTypeUnknown = 0x00,
    spdTypeDDR1 = 0x07,
    spdTypeDDR2 = 0x08,
    spdTypeFBDIMM = 0x09,
    spdTypeDDR3 = 0x0B,
    spdTypeDDR4 = 0x0C,
    spdTypeDDR5 = 0x12,
};

enum class ModuleType: uint8_t
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

struct MFRIDJEP106 {
    uint8_t bankNumber;
    uint8_t OffsetInBank;
};

class SPD
{
  public:
    // Minimal required SPD size.
    // byte[0] == SPD size, byte[2] == SPD type(DDR1/2/3/4/5 etc..)
    static constexpr size_t kMinRequiredSPDSize = 3;

    // Returns the type of the SPD, or SPD::SPDType::spdTypeUnknown on error.
    static SPDType getSPDType(std::span<const uint8_t> spdImage);

    // Returns the total size of SPD, or -1 on error.
    static int32_t getSPDSize(std::span<const uint8_t> spdImage);

    // Parses SPD information from the given source and returns a new SPD object
    // owned by the caller. Returns NULL on error or if the type of the given
    // SPD is unsupported.
    static std::unique_ptr<SPD> getFromImage(std::span<const uint8_t> spdImage);

    // Calculate the crc16 for the SPD content. The crc16 algorithm is defined
    // in JEDEC
    static uint16_t calcJedecCRC16(std::span<const uint8_t> spdImage);

    // Convert SPD type to string
    static std::string spdTypeToStr(SPDType type);

    virtual ~SPD() = default;

    // Returns the type of this SPD, or kTypeUnknown if the SPD type is unknown.
    virtual SPDType type() const = 0;

    // Returns a copy of the raw bytes of this SPD.
    virtual std::span<const uint8_t> raw() const = 0;

    // Returns the number of bytes used by the module manufacturer in this SPD.
    virtual int32_t spdUsedSize() const = 0;

    // Returns the total size of the serial memory used to hold the SPD data.
    virtual int32_t spdTotalSize() const = 0;

    // Manufacturer ID output can be interpreted according to JEDEC's
    // manufacturer ID list (JEP106).
    virtual MFRIDJEP106 manufacturerId() const = 0;
    virtual int8_t manufacturerLocation() const = 0;

    // In the SPD spec, the manufacturingDate is composed by 2 bytes.
    // 1 byte for years and 1 byte for "week in the year"
    // We convert that into std::chrono::time_point by useing the beginning of that year + std::chrono::weeks(week)
    // e.g. year=2022, week=4 will be converted to time_point "01/29/2022 00:00:00"
    virtual std::chrono::time_point<std::chrono::system_clock> manufacturingDate() const = 0;
    virtual ModuleType module() const = 0;
    virtual std::vector<uint8_t> serialNumber() const = 0;
    virtual std::string partNumber() const = 0;
    virtual int8_t revisionCode() const = 0;
    virtual int64_t dimmSize() const = 0; // in MiB
    virtual int8_t eccBits() const = 0;
    virtual int32_t banks() const = 0;
    virtual int32_t rows() const = 0;
    virtual int32_t columns() const = 0;
    // Returns the total number of logical ranks of the DIMM.
    virtual int32_t ranks() const = 0;
    virtual int32_t width() const = 0;
    virtual uint16_t checksum() const = 0; // Stored (not computed) checksum.
};
