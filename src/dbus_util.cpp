#include "dbus_util.hpp"

#include <algorithm>
#include <functional>

namespace dbus_util
{

static bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

static bool isAlpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static bool isValidDBusPathSegmentChar(char c)
{
    return isDigit(c) || isAlpha(c);
}

std::string sanitizeForDBusPathSegment(std::string_view str)
{
    std::string result(str);
    std::ranges::replace_if(result, std::not_fn(isValidDBusPathSegmentChar),
                            '_');
    return result;
}

static bool canAppendToDBusInterface(char c, bool prevDot)
{
    if (prevDot)
    {
        return isAlpha(c) || c == '_';
    }
    return isAlpha(c) || isDigit(c) || (c == '.') || c == '_';
}

bool validateDBusInterfaceSegments(const std::string& str)
{
    bool prevDot = true;
    for (const char& c : str)
    {
        if (!canAppendToDBusInterface(c, prevDot))
        {
            return false;
        }
        prevDot = c == '.';
    }
    return !prevDot;
}

} // namespace dbus_util
