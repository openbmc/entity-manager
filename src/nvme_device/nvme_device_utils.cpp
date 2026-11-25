// SPDX-License-Identifier: Apache-2.0

#include "nvme_device_utils.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>

namespace nvme
{

bool isValidObjectPathElement(std::string_view name)
{
    return !name.empty() && std::ranges::all_of(name, [](unsigned char c) {
        return (std::isalnum(c) != 0) || c == '_';
    });
}

static std::optional<uint32_t> getUint(const nlohmann::json& item,
                                       const char* key, uint32_t max)
{
    auto it = item.find(key);
    if (it == item.end() || !it->is_number_unsigned())
    {
        return std::nullopt;
    }
    auto value = it->get<uint64_t>();
    if (value > max)
    {
        return std::nullopt;
    }
    return static_cast<uint32_t>(value);
}

ConfigResult parseConfig(std::string_view json)
{
    ConfigResult result;
    nlohmann::json root = nlohmann::json::parse(json, nullptr, false);
    if (root.is_discarded() || !root.is_object())
    {
        result.errors.emplace_back("malformed JSON document");
        return result;
    }
    auto cfg = root.find("config");
    if (cfg == root.end() || !cfg->is_array())
    {
        result.errors.emplace_back("missing \"config\" array");
        return result;
    }

    std::set<uint32_t> indexes;
    std::set<std::string> names;
    std::set<uint16_t> buses;

    size_t pos = 0;
    for (const auto& item : *cfg)
    {
        std::string where = "config[" + std::to_string(pos++) + "]";
        if (!item.is_object())
        {
            result.errors.push_back(where + ": not an object");
            continue;
        }
        auto index =
            getUint(item, "Index", std::numeric_limits<uint32_t>::max());
        if (!index)
        {
            result.errors.push_back(where + ": invalid or missing \"Index\"");
            continue;
        }
        auto bus = getUint(item, "BusID", std::numeric_limits<uint16_t>::max());
        if (!bus)
        {
            result.errors.push_back(where + ": invalid or missing \"BusID\"");
            continue;
        }
        std::string name = "nvme" + std::to_string(*index);
        auto nameIt = item.find("Name");
        if (nameIt != item.end())
        {
            if (!nameIt->is_string() ||
                !isValidObjectPathElement(nameIt->get<std::string>()))
            {
                result.errors.push_back(where + ": invalid \"Name\"");
                continue;
            }
            name = nameIt->get<std::string>();
        }
        if (indexes.contains(*index) || names.contains(name) ||
            buses.contains(static_cast<uint16_t>(*bus)))
        {
            result.errors.push_back(where + ": duplicate Index, Name or BusID");
            continue;
        }
        indexes.insert(*index);
        names.insert(name);
        buses.insert(static_cast<uint16_t>(*bus));
        result.entries.push_back(
            {*index, std::move(name), static_cast<uint16_t>(*bus)});
    }
    return result;
}

ConfigResult loadConfig(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
    {
        ConfigResult result;
        result.errors.emplace_back("cannot read file");
        return result;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return parseConfig(ss.str());
}

uint16_t decodeVendorId(uint16_t smbusWord)
{
    return static_cast<uint16_t>((smbusWord << 8) | (smbusWord >> 8));
}

std::string_view vendorName(uint16_t vendorId)
{
    switch (vendorId)
    {
        case 0x8086:
            return "Intel";
        case 0x1e0f:
            return "Kioxia";
        case 0x1344:
            return "Micron";
        case 0x144d:
            return "Samsung";
        case 0x15b7:
            return "SanDisk";
        case 0x1b96:
            return "Western Digital";
        default:
            return "";
    }
}

std::string trimSerial(std::span<const uint8_t> raw)
{
    size_t len = raw.size();
    while (len > 0 && (raw[len - 1] == 0 || raw[len - 1] == ' '))
    {
        len--;
    }
    for (size_t i = 0; i < len; i++)
    {
        if (raw[i] < 0x20 || raw[i] > 0x7e)
        {
            return {};
        }
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    return {reinterpret_cast<const char*>(raw.data()), len};
}

bool validOcpResponse(uint8_t command, std::span<const uint8_t> response,
                      size_t payloadLength)
{
    if (response.size() != payloadLength + 2 || response[0] != payloadLength)
    {
        return false;
    }

    uint8_t crc = 0;
    auto update = [&crc](uint8_t byte) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit)
        {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                               : static_cast<uint8_t>(crc << 1);
        }
    };
    update(nvmeMiAddress << 1);
    update(command);
    update((nvmeMiAddress << 1) | 1);
    for (uint8_t byte : response.first(response.size() - 1))
    {
        update(byte);
    }
    return crc == response.back();
}

std::optional<OcpProperties> decodeOcpResponse(
    uint8_t command, std::span<const uint8_t> response)
{
    size_t length;
    switch (command)
    {
        case ocpGuidCommand:
            length = ocpGuidPayloadLength;
            break;
        case ocpPowerCapacityCommand:
            length = ocpPowerCapacityPayloadLength;
            break;
        case ocpFirmwareModelCommand:
            length = ocpFirmwareModelPayloadLength;
            break;
        case ocpDataFormatVersionCommand:
            length = ocpDataFormatVersionPayloadLength;
            break;
        default:
            return std::nullopt;
    }
    if (!validOcpResponse(command, response, length))
    {
        return std::nullopt;
    }

    auto bigEndian = [](std::span<const uint8_t> bytes) {
        uint64_t value = 0;
        for (uint8_t byte : bytes)
        {
            value = (value << 8) | byte;
        }
        return value;
    };
    OcpProperties properties;
    switch (command)
    {
        case ocpGuidCommand:
            properties.emplace("OcpDataFormatGuid",
                               std::vector<uint8_t>(response.begin() + 1,
                                                    response.begin() + 17));
            break;
        case ocpPowerCapacityCommand:
            properties.emplace("MaxPowerWatts", response[2]);
            properties.emplace(
                "TotalNvmCapacityGB",
                static_cast<uint32_t>(bigEndian(response.subspan(35, 4))));
            break;
        case ocpFirmwareModelCommand:
        {
            auto firmware = trimSerial(response.subspan(1, 8));
            auto model = trimSerial(response.subspan(17, 40));
            if (firmware.empty() || model.empty())
            {
                return std::nullopt;
            }
            properties.emplace("FirmwareVersion", std::move(firmware));
            properties.emplace("SecurityVersion",
                               bigEndian(response.subspan(9, 8)));
            properties.emplace("ModelNumber", std::move(model));
            break;
        }
        case ocpDataFormatVersionCommand:
            properties.emplace(
                "DataFormatVersion",
                static_cast<uint16_t>(bigEndian(response.subspan(1, 2))));
            break;
    }
    return properties;
}

void applyScan(std::optional<DeviceState>& state,
               const std::optional<Identity>& identity,
               const std::function<VpdResult()>& readVpd)
{
    if (!identity)
    {
        state.reset();
        return;
    }
    if (state && !(state->identity == *identity))
    {
        state.reset();
    }
    if (!state)
    {
        state = DeviceState{*identity, {}, {}};
    }
    auto vpd = readVpd();
    if (vpd)
    {
        state->vpd = std::move(*vpd);
    }
}

} // namespace nvme
