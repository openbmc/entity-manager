// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once

#include <nlohmann/json.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <optional>

struct InvAddRemoveInfo
{
    std::string model = "Unknown";
    std::string type = "Unknown";
    std::string sn = "Unknown";
    std::string name = "Unknown";
};

void logDeviceAdded(const nlohmann::json& record);

void logDeviceRemoved(const nlohmann::json& record);

InvAddRemoveInfo queryInvInfo(const nlohmann::json& record);

// @brief        the inventory object path entity-manager exports for a
//               configuration record, matching the path built by
//               EntityManager::postBoardToDBus()
// @param record a configuration record
// @returns      the object path, or nullopt if the record has no usable Name
std::optional<sdbusplus::object_path> inventoryPath(
    const nlohmann::json& record);
