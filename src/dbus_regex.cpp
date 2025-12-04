#include "dbus_regex.hpp"

#include <regex>

namespace dbus_regex
{

static const std::regex illegalDbusMemberRegex("[^A-Za-z0-9_]");
static const std::regex illegalDbusPathRegex("[^A-Za-z0-9_.]");

static void sanitizeInPlace(std::string& str, const std::regex& regex)
{
    str = std::regex_replace(str, regex, "_");
}

void sanitizeMember(std::string& str)
{
    sanitizeInPlace(str, illegalDbusMemberRegex);
}

void sanitizePath(std::string& str)
{
    sanitizeInPlace(str, illegalDbusPathRegex);
}

} // namespace dbus_regex
