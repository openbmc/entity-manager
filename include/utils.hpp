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
/// \file utils.hpp

#pragma once

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/exception.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>

constexpr const char* configurationOutDir = "/var/configuration/";
constexpr const char* versionHashFile = "/var/configuration/version";
constexpr const char* versionFile = "/etc/os-release";

extern boost::asio::io_context io;

using DBusValueVariant =
    std::variant<std::string, int64_t, uint64_t, double, int32_t, uint32_t,
                 int16_t, uint16_t, uint8_t, bool, std::vector<uint8_t>>;
using DBusInterface = boost::container::flat_map<std::string, DBusValueVariant>;
using DBusObject = boost::container::flat_map<std::string, DBusInterface>;
using MapperGetSubTreeResponse =
    boost::container::flat_map<std::string, DBusObject>;

namespace properties
{
constexpr const char* interface = "org.freedesktop.DBus.Properties";
constexpr const char* get = "Get";
} // namespace properties

namespace power
{
const static constexpr char* busname = "xyz.openbmc_project.State.Host";
const static constexpr char* interface = "xyz.openbmc_project.State.Host";
const static constexpr char* path = "/xyz/openbmc_project/state/host0";
const static constexpr char* property = "CurrentHostState";
} // namespace power

bool findFiles(const std::filesystem::path& dirPath,
               const std::string& matchString,
               std::vector<std::filesystem::path>& foundPaths);
bool findFiles(const std::vector<std::filesystem::path>&& dirPaths,
               const std::string& matchString,
               std::vector<std::filesystem::path>& foundPaths);

bool getI2cDevicePaths(
    const std::filesystem::path& dirPath,
    boost::container::flat_map<size_t, std::filesystem::path>& busPaths);

bool validateJson(const nlohmann::json& schemaFile,
                  const nlohmann::json& input);

bool isPowerOn(void);
void setupPowerMatch(const std::shared_ptr<sdbusplus::asio::connection>& conn);
struct DBusInternalError final : public sdbusplus::exception_t
{
    const char* name() const noexcept override
    {
        return "org.freedesktop.DBus.Error.Failed";
    }
    const char* description() const noexcept override
    {
        return "internal error";
    }
    const char* what() const noexcept override
    {
        return "org.freedesktop.DBus.Error.Failed: "
               "internal error";
    }

    int get_errno() const noexcept override
    {
        return EACCES;
    }
};

inline bool fwVersionIsSame(void)
{
    std::ifstream version(versionFile);
    if (!version.good())
    {
        std::cerr << "Can't read " << versionFile << "\n";
        return false;
    }

    std::string versionData;
    std::string line;
    while (std::getline(version, line))
    {
        versionData += line;
    }

    std::string expectedHash =
        std::to_string(std::hash<std::string>{}(versionData));

    std::filesystem::create_directory(configurationOutDir);
    std::ifstream hashFile(versionHashFile);
    if (hashFile.good())
    {

        std::string hashString;
        hashFile >> hashString;

        if (expectedHash == hashString)
        {
            return true;
        }
        hashFile.close();
    }

    std::ofstream output(versionHashFile);
    output << expectedHash;
    return false;
}

std::optional<std::string> templateCharReplace(
    nlohmann::json::iterator& keyPair, const DBusObject& object,
    const size_t index,
    const std::optional<std::string>& replaceStr = std::nullopt);

std::optional<std::string> templateCharReplace(
    nlohmann::json::iterator& keyPair, const DBusInterface& interface,
    const size_t index,
    const std::optional<std::string>& replaceStr = std::nullopt);

inline bool deviceHasLogging(const nlohmann::json& json)
{
    auto logging = json.find("Logging");
    if (logging != json.end())
    {
        auto ptr = logging->get_ptr<const std::string*>();
        if (ptr)
        {
            if (*ptr == "Off")
            {
                return false;
            }
        }
    }
    return true;
}

/// \brief Match a Dbus property against a probe statement.
/// \param probe the probe statement to match against.
/// \param dbusValue the property value being matched to a probe.
/// \return true if the dbusValue matched the probe otherwise false.
bool matchProbe(const nlohmann::json& probe, const DBusValueVariant& dbusValue);
