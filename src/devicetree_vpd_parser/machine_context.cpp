// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2024 Hewlett Packard Enterprise

#include "machine_context.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>

namespace
{
std::optional<std::string> readDeviceTreeNode(const std::filesystem::path& path)
{
    errno = 0;
    std::ifstream stream(path);
    if (!stream.is_open())
    {
        int error = errno;
        if (error != ENOENT)
        {
            lg2::warning("Failed to open device-tree node {PATH}: {ERROR}",
                         "PATH", path.string(), "ERROR",
                         (error != 0) ? std::strerror(error) : "unknown error");
        }
        return std::nullopt;
    }

    std::string value;
    if (!std::getline(stream, value))
    {
        if (stream.bad())
        {
            lg2::warning("Failed to read device-tree node {PATH}", "PATH",
                         path.string());
        }
        return std::nullopt;
    }

    return value;
}
} // namespace

void MachineContext::populateFromDeviceTree(
    const std::filesystem::path& nodeBase)
{
    if (auto value = readDeviceTreeNode(nodeBase / "model"))
    {
        MachineContext::Asset::model(*value);
    }

    if (auto value = readDeviceTreeNode(nodeBase / "serial-number"))
    {
        MachineContext::Asset::serial_number(*value);
    }

    if (auto value = readDeviceTreeNode(nodeBase / "part-number"))
    {
        MachineContext::Asset::part_number(*value);
    }

    if (auto value = readDeviceTreeNode(nodeBase / "manufacturer"))
    {
        MachineContext::Asset::manufacturer(*value);
    }
};

bool MachineContext::keyNodeExists()
{
    std::filesystem::path nodePath{nodeBasePath + std::string("model")};

    return std::filesystem::exists(nodePath);
};
