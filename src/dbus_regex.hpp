#pragma once

#include <string>

namespace dbus_regex
{

std::string sanitizeForDBusMember(const std::string& str);
std::string sanitizeForDBusPath(const std::string& str);

} // namespace dbus_regex
