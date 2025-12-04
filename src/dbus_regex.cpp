#include "dbus_regex.hpp"

#include <regex>

namespace dbus_regex
{

const std::regex illegalDbusPathRegex("[^A-Za-z0-9_.]");
const std::regex illegalDbusMemberRegex("[^A-Za-z0-9_]");

} // namespace dbus_regex
