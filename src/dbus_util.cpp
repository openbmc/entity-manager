#include "dbus_util.hpp"

#include <algorithm>
#include <cctype>
#include <functional>

namespace dbus_util
{

static bool isValidDBusPathSegmentChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

std::string sanitizeForDBusPathSegment(const std::string& str)
{
    if (str.empty())
    {
        return "_";
    }
    std::string result = str;
    std::ranges::replace_if(result, std::not_fn(isValidDBusPathSegmentChar),
                            '_');
    return result;
}

static char appendToDBusInterface(char c, bool prevDot)
{
    const bool alpha = std::isalpha(static_cast<unsigned char>(c)) != 0;
    const bool digit = std::isdigit(static_cast<unsigned char>(c)) != 0;
    if (prevDot)
    {
        if (alpha)
        {
            return c;
        }
        return '_';
    }
    if (alpha || digit || (c == '.'))
    {
        return c;
    }
    return '_';
}

std::string sanitizeForDBusInterface(const std::string& str)
{
    std::string result;

    // allow for possible _ append on empty string
    result.reserve(str.size() + 1);

    bool prevDot = true;
    for (const char& c : str)
    {
        const char out = appendToDBusInterface(c, prevDot);
        result.push_back(out);
        prevDot = out == '.';
    }
    if (prevDot)
    {
        result.push_back('_');
    }

    return result;
}

} // namespace dbus_util
