#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

class Spd
{
  public:
    enum class SpdType
    {
        kTypeUnknown = 0x00,
        kTypeDdr1 = 0x07,
        kTypeDdr2 = 0x08,
        kTypeFbdimm = 0x09,
        kTypeDdr3 = 0x0B,
        kTypeDdr4 = 0x0C,
        kTypeDdr5 = 0x12,
    };

    enum class ModuleType
    {
        kTypeUnkonwn = 0x00,
        kTypeRDIMM = 0x01,
        kTypeUDIMM = 0x02,
        kTypeSODIMM = 0x03,
        kTypeLRDIMM = 0x04,
        kTypeDDIMM = 0x0A,
        kTypeSolderDown = 0x0B,
        kTypeNVNRDIMM = 0x91,
        kTypeNVPRDIMM = 0x92,
    };

    // Minimal required SPD size.
    // byte[0] == SPD size, byte[2] == SPD type(DDR1/2/3/4/5 etc..)
    static constexpr uint32_t kMinRequiredSpdSize = 3;

    // Returns the type of the SPD, or Spd::kTypeUnknown on error.
    static SpdType getSpdType(std::span<const uint8_t> spdImage);

    // Returns the total size of SPD, or -1 on error.
    static int32_t getSpdSize(std::span<const uint8_t> spdImage);

    // Parses SPD information from the given source and returns a new Spd object
    // owned by the caller. Returns NULL on error or if the type of the given
    // SPD is unsupported.
    static std::unique_ptr<Spd> getFromImage(std::span<const uint8_t> spdImage);

    // Calculate the crc16 for the SPD content. The crc16 algorithm is defined
    // in JEDEC
    static uint16_t calJedecCrc16(std::span<const uint8_t> spdImage);

    virtual ~Spd() = default;

    // Returns the type of this Spd, or kTypeUnknown if the Spd has not been
    // initialized with FillFromImage or the type is unknown.
    virtual SpdType type() const = 0;

    // Returns a copy of the raw bytes of this Spd.
    virtual std::span<const uint8_t> raw() const = 0;

    // Returns the number of bytes used by the module manufacturer in this Spd.
    virtual int32_t spdUsedSize() const = 0;

    // Returns the total size of the serial memory used to hold the Spd data.
    virtual int32_t spdTotalSize() const = 0;

    // Manufacturer ID output can be interpreted according to JEDEC's
    // manufacturer ID list (JEP106).
    // pair.first == bank number, pair.second == offset in the bank.
    virtual std::pair<int8_t, int8_t> manufacturerId() const = 0;
    virtual int8_t manufacturerLocation() const = 0;

    // pair.first == the full year number (e.g. 2006)
    // pair.second == the number of the week in the year (e.g. 14).
    virtual std::pair<int32_t, int32_t> manufacturingDate() const = 0;
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
