#pragma once

#include <string>

namespace dbus_util
{

// @brief      replaces invalid characters with '_'
// @param str  any non-empty string
// @returns    a valid D-Bus path segment as per
//             https://dbus.freedesktop.org/doc/dbus-specification.html#basic-types
//             Section 'Valid Object Paths'
std::string sanitizeForDBusPathSegment(std::string_view str);

// @brief      checks for valid D-Bus interface segments
// @param str  any string
// @returns    true in case of valid D-Bus interface segment(s) as per
//             https://dbus.freedesktop.org/doc/dbus-specification.html#basic-types
//             Section 'Valid Names -> Interface names'
bool validateDBusInterfaceSegments(const std::string& str);

} // namespace dbus_util
