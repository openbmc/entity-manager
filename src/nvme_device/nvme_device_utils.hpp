// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace nvme
{
constexpr uint8_t nvmeMiAddress = 0x6a;
constexpr uint8_t nvmeVpdAddress = 0x53;
// OCP Datacenter NVMe SSD Specification v2.7, section 11.2.
constexpr uint8_t ocpGuidCommand = 0x20;
constexpr uint8_t ocpPowerCapacityCommand = 0x32;
constexpr uint8_t ocpFirmwareModelCommand = 0x60;
constexpr uint8_t ocpDataFormatVersionCommand = 0xf8;
constexpr size_t ocpGuidPayloadLength = 16;
constexpr size_t ocpPowerCapacityPayloadLength = 38;
constexpr size_t ocpFirmwareModelPayloadLength = 56;
constexpr size_t ocpDataFormatVersionPayloadLength = 6;
constexpr const char* defaultConfigPath =
    "/usr/share/entity-manager/nvme_device.json";

struct ConfigEntry
{
    uint32_t index = 0;
    std::string name;
    uint16_t bus = 0;

    bool operator==(const ConfigEntry&) const = default;
};

struct ConfigResult
{
    std::vector<ConfigEntry> entries;
    // One human readable message per rejected item.
    std::vector<std::string> errors;
};

/// Valid D-Bus object path element: non-empty, [A-Za-z0-9_] only.
bool isValidObjectPathElement(std::string_view name);

/// Parse the JSON configuration. Invalid entries are skipped and reported in
/// ConfigResult::errors; this never throws.
ConfigResult parseConfig(std::string_view json);

/// Read and parse a configuration file. A missing/unreadable file results in
/// no entries and a single error.
ConfigResult loadConfig(const std::string& path);

/// NVMe-MI transmits the vendor ID most significant byte first, whereas an
/// SMBus word read returns the first byte in the low half.
uint16_t decodeVendorId(uint16_t smbusWord);

/// Name of a supported vendor, empty for unknown IDs.
std::string_view vendorName(uint16_t vendorId);

/// Remove trailing padding (NUL / space), keeping inner characters.
/// Returns empty if any remaining byte is not printable ASCII.
std::string trimSerial(std::span<const uint8_t> raw);

bool validOcpResponse(uint8_t command, std::span<const uint8_t> response,
                      size_t payloadLength);

using OcpValue = std::variant<uint8_t, uint16_t, uint32_t, uint64_t,
                              std::string, std::vector<uint8_t>>;
using OcpProperties = std::map<std::string, OcpValue>;

std::optional<OcpProperties> decodeOcpResponse(
    uint8_t command, std::span<const uint8_t> response);

struct Identity
{
    uint16_t vendorId = 0;
    std::string serial;

    bool operator==(const Identity&) const = default;
};

using VpdMap = std::map<std::string, std::string>;

struct DeviceState
{
    Identity identity;
    VpdMap vpd;
    std::map<uint8_t, OcpProperties> ocp;

    bool supportVpd() const
    {
        return !vpd.empty();
    }
};

/// Result of reading the VPD: nullopt on read failure, an empty map when the
/// device has no usable VPD.
using VpdResult = std::optional<VpdMap>;

/// Apply one scan result to the stored state of a device.
///  - identity == nullopt (read failure / device gone): state is removed.
///  - identity changed: old VPD is dropped before the new one is read.
///  - VPD read failure keeps previously read VPD.
/// readVpd is only invoked when the identity is valid.
void applyScan(std::optional<DeviceState>& state,
               const std::optional<Identity>& identity,
               const std::function<VpdResult()>& readVpd);

} // namespace nvme
