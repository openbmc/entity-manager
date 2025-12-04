#include "dbus_regex.hpp"

#include <regex>

namespace dbus_regex
{

std::string sanitizeForDBusMember(const std::string& str)
{
    static const std::regex illegalDbusMemberRegex("[^A-Za-z0-9_]");
    return std::regex_replace(str, illegalDbusMemberRegex, "_");
}

std::string sanitizeForDBusPath(const std::string& str)
{
    static const std::regex illegalDbusPathRegex("[^A-Za-z0-9_.]");
    return std::regex_replace(str, illegalDbusPathRegex, "_");
}

} // namespace dbus_regex
