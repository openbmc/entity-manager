// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#pragma once

#include "dbus_interface.hpp"

#include <nlohmann/json.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <cstdint>
#include <string>

namespace mctp_target
{

// Implements xyz.openbmc_project.Configuration.MCTPTarget on board objects.
class MCTPTarget
{
  public:
    explicit MCTPTarget(dbus_interface::EMDBusInterface& dbusInterface);

    void createMethods(const std::string& boardId,
                       const sdbusplus::object_path& path,
                       nlohmann::json& systemConfiguration,
                       const std::string& board);

    sdbusplus::object_path addI2CTarget(
        const std::string& name, uint64_t bus, uint8_t address,
        nlohmann::json& systemConfiguration, const std::string& boardId,
        const sdbusplus::object_path& path, const std::string& board);

    void removeI2CTarget(const std::string& name,
                         nlohmann::json& systemConfiguration,
                         const std::string& boardId,
                         const sdbusplus::object_path& path,
                         const std::string& board);

  private:
    dbus_interface::EMDBusInterface& dbusInterface;
};

} // namespace mctp_target
