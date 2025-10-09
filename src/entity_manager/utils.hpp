#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

using DBusValueVariant =
    std::variant<std::string, int64_t, uint64_t, double, int32_t, uint32_t,
                 int16_t, uint16_t, uint8_t, bool, std::vector<uint8_t>>;
using DBusInterface = boost::container::flat_map<std::string, DBusValueVariant>;
using DBusObject = boost::container::flat_map<std::string, DBusInterface>;

constexpr const char* configurationOutDir = "/var/configuration/";
constexpr const char* versionHashFile = "/var/configuration/version";
constexpr const char* versionFile = "/etc/os-release";

namespace em_utils
{

namespace properties
{
constexpr const char* interface = "org.freedesktop.DBus.Properties";
constexpr const char* get = "Get";
} // namespace properties

bool fwVersionIsSame();

void handleLeftOverTemplateVars(nlohmann::json::iterator& keyPair);

std::optional<std::string> templateCharReplace(
    nlohmann::json::iterator& keyPair, const DBusObject& object, size_t index,
    const std::optional<std::string>& replaceStr = std::nullopt,
    bool handleLeftOver = true);

std::optional<std::string> templateCharReplace(
    nlohmann::json::iterator& keyPair, const DBusInterface& interface,
    size_t index, const std::optional<std::string>& replaceStr = std::nullopt);

std::string buildInventorySystemPath(std::string& boardName,
                                     const std::string& boardType);

} // namespace em_utils
