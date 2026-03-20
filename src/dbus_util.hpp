#pragma once

#include <string>

namespace dbus_util
{

// @brief      replaces invalid characters with '_'
// @param str  any string
// @returns    a valid D-Bus path segment as per
//             https://dbus.freedesktop.org/doc/dbus-specification.html#basic-types
//             Section 'Valid Object Paths'
std::string sanitizeForDBusPathSegment(const std::string& str);

// @brief      replaces invalid characters with '_'
// @param str  any string
// @returns    one or more valid D-Bus interface segment(s) as per
//             https://dbus.freedesktop.org/doc/dbus-specification.html#basic-types
//             Section 'Valid Names -> Interface names'
std::string sanitizeForDBusInterface(const std::string& str);

} // namespace dbus_util
