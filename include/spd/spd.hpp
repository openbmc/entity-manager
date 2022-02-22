// A standalone library for parsing DIMM SPDs.
#pragma once

#include <cstdint>
#include <memory>
#include <span>
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
        kTypeDdr3 = 0x0b,
        kTypeDdr4 = 0x0c,
        kTypeNvm = 0x0d,
        kTypeDdr5 = 0x12,
    };

    enum class ModuleType
    {
        kTypeUnkonwn,
        kTypeRDIMM,
        kTypeUDIMM,
        kTypeSODIMM,
        kTypeMICRODIMM,
        kTypeMINIRDIMM,
        kTypeMINIUDIMM,
        kTypeMINICDIMM,
        kType72BSODIMM,
        kType72BSORDIMM,
        kType72BSOCDIMM,
        kTypeLRDIMM,
        kType16BSODIMM,
        kType32BSODIMM,
        kTypeNVRDIMM,
        kTypeNHNVLRDIMM,
        kTypeNVMNHYBRID,
    };

    enum class EccType
    {
        kTypeUnkonwn,
        kTypeRDIMM,
        kTypeUDIMM,
        kTypeSODIMM,
        kTypeMICRODIMM,
        kTypeMINIRDIMM,
        kTypeMINIUDIMM,
        kTypeMINICDIMM,
        kType72BSODIMM,
        kType72BSORDIMM,
        kType72BSOCDIMM,
        kTypeLRDIMM,
        kType16BSODIMM,
        kType32BSODIMM,
        kTypeNVRDIMM,
        kTypeNHNVLRDIMM,
        kTypeNVMNHYBRID,
    };

    // Minimal required SPD size.
    // byte[0] == SPD size, byte[2] == SPD type(DDR1/2/3/4/5 etc..)
    static constexpr uint32_t kMinRequiredSpdSize = 3;

    // Returns the type of the SPD, or Spd::kTypeUnknown on error.
    static SpdType getSpdType(std::span<const uint8_t> spd_image);

    // Parses SPD information from the given source and returns a new Spd object
    // owned by the caller. Returns NULL on error or if the type of the given
    // SPD is unsupported.
    static std::unique_ptr<Spd>
        getFromImage(std::span<const uint8_t> spd_image);

    // Calculate the crc16 for the SPD content. The crc16 algorithm is defined
    // in JEDEC
    static uint16_t calJedecCrc16(std::span<const uint8_t> spd_image);

    virtual ~Spd() = default;

    // Returns the type of this Spd, or kTypeUnknown if the Spd has not been
    // initialized with FillFromImage or the type is unknown.
    virtual SpdType type() const = 0;

    // Returns a copy of the raw bytes of this Spd.
    virtual std::span<const uint8_t> raw() const = 0;

    // Returns the number of bytes used by the module manufacturer in this Spd.
    virtual size_t spdUsedSize() const = 0;

    // Returns the total size of the serial memory used to hold the Spd data.
    virtual size_t spdTotalSize() const = 0;

    // Manufacturer ID output can be interpreted according to JEDEC's
    // manufacturer ID list (JEP106).
    // pair.first == bank number, pair.second == offset in the bank.
    virtual std::pair<uint8_t, uint8_t> manufacturerId() const = 0;
    virtual uint8_t manufacturerLocation() const = 0;

    // pair.first == the full year number (e.g. 2006)
    // pair.second == the number of the week in the year (e.g. 14).
    virtual std::pair<uint32_t, uint32_t> manufacturingDate() const = 0;
    virtual ModuleType module() const = 0;
    virtual std::vector<uint8_t> serialNumber() const = 0;
    virtual std::vector<uint8_t> partNumber() const = 0;
    virtual uint8_t revisionCode() const = 0;
    virtual uint64_t dimmSize() const = 0; // in MiB
    virtual uint8_t eccBits() const = 0;
    virtual uint32_t banks() const = 0;
    virtual uint32_t rows() const = 0;
    virtual uint32_t columns() const = 0;
    // Returns the total number of logical ranks of the DIMM.
    virtual uint32_t ranks() const = 0;
    virtual uint32_t width() const = 0;
    virtual uint16_t checksum() const = 0; // Stored (not computed) checksum.
};
