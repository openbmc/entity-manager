#pragma once

#include <string>

namespace dbus_regex
{

std::string sanitizeMember(const std::string& str);
std::string sanitizePath(const std::string& str);

} // namespace dbus_regex
