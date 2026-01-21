#pragma once

#include "../utils.hpp"

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

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

/**
 * @brief API to find and replace @PROPERTYNAME in JSON
 * This API checks if the replacementTarget string has @patterned value,
 * something like @PROPERTYNAME. If the find is successful, it replaces
 * @PROPERTYNAME with the property value.
 *
 * @param[in, out] replacementTarget - Target string to find and replace the
 * @patterned value.
 * @param[in] propName - Name of the property.
 * @param[in] propVal - Value of the property.
 */
void findAndReplaceAtCommand(std::string* replacementTarget,
                             const std::string& propName,
                             const DBusValueVariant& propVal);
} // namespace em_utils
