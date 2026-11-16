#pragma once

#include <string>

namespace dbus_util
{

// @brief      replaces invalid characters with '_'
// @param str  any non-empty string
// @returns    a valid D-Bus path segment as per Section 'Valid Object Paths':
// https://dbus.freedesktop.org/doc/dbus-specification.html#basic-types
std::string sanitizeForDBusPathSegment(std::string_view str);

// @brief      checks for valid D-Bus interface segments
// @param str  any string
// @returns    true for valid D-Bus interface segments as per
//             Section 'Valid Names -> Interface names':
// https://dbus.freedesktop.org/doc/dbus-specification.html#basic-types
bool validateDBusInterfaceSegments(const std::string& str);

} // namespace dbus_util
