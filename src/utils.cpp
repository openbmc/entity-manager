/*
// Copyright (c) 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
/// \file utils.cpp

#include "utils.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/lexical_cast.hpp>
#include <sdbusplus/bus/match.hpp>

#include <filesystem>
#include <iostream>
#include <map>
#include <regex>

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

        for (const auto& p : fs::directory_iterator(dirPath))
        {
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
                std::cerr << "Syntax error in regular expression: " << probe
                          << " will never match";
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
