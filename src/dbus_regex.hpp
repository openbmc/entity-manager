#pragma once

#include <string>

namespace dbus_regex
{

void sanitizeMember(std::string& str);
void sanitizePath(std::string& str);

} // namespace dbus_regex
