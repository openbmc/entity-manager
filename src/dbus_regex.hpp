#pragma once

#include <regex>

namespace dbus_regex
{

extern const std::regex illegalDbusPathRegex;
extern const std::regex illegalDbusMemberRegex;

void sanitizeMemberInPlace(std::string& str);
void sanitizePathInPlace(std::string& str);

} // namespace dbus_regex
