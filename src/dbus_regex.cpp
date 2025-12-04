#include "dbus_regex.hpp"

#include <regex>

namespace dbus_regex
{

static void sanitizeInPlace(std::string& str, const std::regex& regex)
{
    std::regex_replace(str.begin(), str.begin(), str.end(), regex, "_");
}

void sanitizeMember(std::string& str)
{
    const std::regex illegalDbusMemberRegex("[^A-Za-z0-9_]");
    sanitizeInPlace(str, illegalDbusMemberRegex);
}

void sanitizePath(std::string& str)
{
    const std::regex illegalDbusPathRegex("[^A-Za-z0-9_.]");
    sanitizeInPlace(str, illegalDbusPathRegex);
}

} // namespace dbus_regex
