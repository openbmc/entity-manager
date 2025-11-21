// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2017 Intel Corporation

#pragma once

#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/exception.hpp>

#include <charconv>
#include <filesystem>
#include <flat_map>

using DBusValueVariant =
    std::variant<std::string, int64_t, uint64_t, double, int32_t, uint32_t,
                 int16_t, uint16_t, uint8_t, bool, std::vector<uint8_t>,
                 std::vector<int64_t>, std::vector<uint64_t>,
                 std::vector<double>, std::vector<std::string>>;
using DBusInterface = std::flat_map<std::string, DBusValueVariant, std::less<>>;
using DBusObject = std::flat_map<std::string, DBusInterface, std::less<>>;
using MapperGetSubTreeResponse =
    std::flat_map<std::string, DBusObject, std::less<>>;
using FirstIndex = size_t;
using LastIndex = size_t;

bool findFiles(const std::filesystem::path& dirPath,
               const std::string& matchString,
               std::vector<std::filesystem::path>& foundPaths);
bool findFiles(const std::vector<std::filesystem::path>& dirPaths,
               const std::string& matchString,
               std::vector<std::filesystem::path>& foundPaths);

bool getI2cDevicePaths(const std::filesystem::path& dirPath,
                       std::flat_map<size_t, std::filesystem::path>& busPaths);

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

inline bool deviceHasLogging(const nlohmann::json& json)
{
    auto logging = json.find("Logging");
    if (logging != json.end())
    {
        const auto* ptr = logging->get_ptr<const std::string*>();
        if (ptr != nullptr)
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

inline char asciiToLower(char c)
{
    // Converts a character to lower case without relying on std::locale
    if ('A' <= c && c <= 'Z')
    {
        c -= static_cast<char>('A' - 'a');
    }
    return c;
}

template <typename T>
auto iFindFirst(T&& str, std::string_view sub)
{
    return std::ranges::search(str, sub, [](char a, char b) {
        return asciiToLower(a) == asciiToLower(b);
    });
}

std::vector<std::string> split(std::string_view str, char delim);

void iReplaceAll(std::string& str, std::string_view search,
                 std::string_view replace);

void replaceAll(std::string& str, std::string_view search,
                std::string_view replace);

std::string toLowerCopy(std::string_view str);

template <typename T>
std::from_chars_result fromCharsWrapper(const std::string_view& str, T& out,
                                        bool& fullMatch, int base = 10)
{
    auto result = std::from_chars(
        str.data(), std::next(str.begin(), str.size()), out, base);

    fullMatch = result.ptr == std::next(str.begin(), str.size());

    return result;
}
