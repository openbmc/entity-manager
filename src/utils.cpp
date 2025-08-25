// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2017 Intel Corporation

#include "utils.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/lexical_cast.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus/match.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <ranges>
#include <regex>
#include <string_view>
#include <utility>

namespace fs = std::filesystem;

bool findFiles(const fs::path& dirPath, const std::string& matchString,
               std::vector<fs::path>& foundPaths)
{
    if (!fs::exists(dirPath))
    {
        return false;
    }

    std::regex search(matchString);
    std::smatch match;
    for (const auto& p : fs::directory_iterator(dirPath))
    {
        std::string path = p.path().string();
        if (std::regex_search(path, match, search))
        {
            foundPaths.emplace_back(p.path());
        }
    }
    return true;
}

bool findFiles(const std::vector<fs::path>&& dirPaths,
               const std::string& matchString,
               std::vector<fs::path>& foundPaths)
{
    std::map<fs::path, fs::path> paths;
    std::regex search(matchString);
    std::smatch match;
    for (const auto& dirPath : dirPaths)
    {
        if (!fs::exists(dirPath))
        {
            continue;
        }

        for (const auto& p : fs::recursive_directory_iterator(dirPath))
        {
            std::error_code ec;
            if (p.is_directory(ec))
            {
                continue;
            }

            std::string path = p.path().string();
            if (std::regex_search(path, match, search))
            {
                paths[p.path().filename()] = p.path();
            }
        }
    }

    for (const auto& [key, value] : paths)
    {
        foundPaths.emplace_back(value);
    }

    return !foundPaths.empty();
}

bool getI2cDevicePaths(const fs::path& dirPath,
                       boost::container::flat_map<size_t, fs::path>& busPaths)
{
    if (!fs::exists(dirPath))
    {
        return false;
    }

    // Regex for matching the path
    std::regex searchPath(std::string(R"(i2c-\d+$)"));
    // Regex for matching the bus numbers
    std::regex searchBus(std::string(R"(\w[^-]*$)"));
    std::smatch matchPath;
    std::smatch matchBus;
    for (const auto& p : fs::directory_iterator(dirPath))
    {
        std::string path = p.path().string();
        if (std::regex_search(path, matchPath, searchPath))
        {
            if (std::regex_search(path, matchBus, searchBus))
            {
                size_t bus = stoul(*matchBus.begin());
                busPaths.insert(std::pair<size_t, fs::path>(bus, p.path()));
            }
        }
    }

    return true;
}

/// \brief JSON/DBus matching Callable for std::variant (visitor)
///
/// Default match JSON/DBus match implementation
/// \tparam T The concrete DBus value type from DBusValueVariant
template <typename T>
struct MatchProbe
{
    /// \param probe the probe statement to match against
    /// \param value the property value being matched to a probe
    /// \return true if the dbusValue matched the probe otherwise false
    static bool match(const nlohmann::json& probe, const T& value)
    {
        return probe == value;
    }
};

/// \brief JSON/DBus matching Callable for std::variant (visitor)
///
/// std::string specialization of MatchProbe enabling regex matching
template <>
struct MatchProbe<std::string>
{
    /// \param probe the probe statement to match against
    /// \param value the string value being matched to a probe
    /// \return true if the dbusValue matched the probe otherwise false
    static bool match(const nlohmann::json& probe, const std::string& value)
    {
        if (probe.is_string())
        {
            try
            {
                std::regex search(probe);
                std::smatch regMatch;
                return std::regex_search(value, regMatch, search);
            }
            catch (const std::regex_error&)
            {
                lg2::error(
                    "Syntax error in regular expression: {PROBE} will never match",
                    "PROBE", probe);
            }
        }

        // Skip calling nlohmann here, since it will never match a non-string
        // to a std::string
        return false;
    }
};

/// \brief Forwarding JSON/DBus matching Callable for std::variant (visitor)
///
/// Forward calls to the correct template instantiation of MatchProbe
struct MatchProbeForwarder
{
    explicit MatchProbeForwarder(const nlohmann::json& probe) : probeRef(probe)
    {}
    const nlohmann::json& probeRef;

    template <typename T>
    bool operator()(const T& dbusValue) const
    {
        return MatchProbe<T>::match(probeRef, dbusValue);
    }
};

bool matchProbe(const nlohmann::json& probe, const DBusValueVariant& dbusValue)
{
    return std::visit(MatchProbeForwarder(probe), dbusValue);
}

inline char asciiToLower(char c)
{
    // Converts a character to lower case without relying on std::locale
    if ('A' <= c && c <= 'Z')
    {
        c -= static_cast<char>('A' - 'a');
    }
    return c;
}

std::pair<FirstIndex, LastIndex> iFindFirst(std::string_view str,
                                            std::string_view sub)
{
    if (sub.empty())
    {
        return {std::string_view::npos, std::string_view::npos};
    }
    auto result = std::ranges::search(str, sub, [](char a, char b) {
        return asciiToLower(a) == asciiToLower(b);
    });

    if (!result.empty())
    {
        size_t start = static_cast<size_t>(
            std::ranges::distance(str.begin(), result.begin()));
        return {start, start + sub.size()};
    }

    return {std::string_view::npos, std::string_view::npos};
}
