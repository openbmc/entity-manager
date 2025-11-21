#pragma once

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <filesystem>
#include <flat_map>

using DBusValueVariant =
    std::variant<std::string, int64_t, uint64_t, double, int32_t, uint32_t,
                 int16_t, uint16_t, uint8_t, bool, std::vector<uint8_t>>;
using DBusInterface = std::flat_map<std::string, DBusValueVariant, std::less<>>;
using DBusObject = std::flat_map<std::string, DBusInterface, std::less<>>;

constexpr const char* versionFile = "/etc/os-release";

namespace em_utils
{

namespace properties
{
constexpr const char* interface = "org.freedesktop.DBus.Properties";
constexpr const char* get = "Get";
} // namespace properties

bool fwVersionIsSame(const std::filesystem::path& configurationOutDir);

void handleLeftOverTemplateVars(nlohmann::json& value);

std::optional<std::string> templateCharReplace(
    nlohmann::json& value, const DBusObject& object, size_t index,
    const std::optional<std::string>& replaceStr = std::nullopt,
    bool handleLeftOver = true);

std::optional<std::string> templateCharReplace(
    nlohmann::json& value, const DBusInterface& interface, size_t index,
    const std::optional<std::string>& replaceStr = std::nullopt);

std::string buildInventorySystemPath(std::string& boardName,
                                     const std::string& boardType);

} // namespace em_utils
