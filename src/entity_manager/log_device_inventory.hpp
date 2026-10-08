// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_set>
#include <vector>

// @brief Model/Type/SerialNumber for the legacy OpenBMC.0.1.* journal
//        message, kept alongside the structured D-Bus event as a temporary
//        compatibility shim.
struct LegacyInvInfo
{
    std::string model = "Unknown";
    std::string type = "Unknown";
    std::string sn = "Unknown";
};

// @brief Selects the records of a derived-new configuration that are to be
//        reported with InventoryAdded.
// @param newConfiguration  records not seen by an earlier scan of this run,
//                          keyed by record name
// @param cachedBaseline    keys of the configuration persisted by a previous
//                          run for which no InventoryRemoved has been reported
//                          since startup; consumers already know these devices
// @returns the keys of the records to report, in iteration order
std::vector<std::string> keysToLogAdded(
    const nlohmann::json& newConfiguration,
    const std::unordered_set<std::string>& cachedBaseline);

void logDeviceAdded(const nlohmann::json& record);

void logDeviceRemoved(const nlohmann::json& record);

std::string queryInvName(const nlohmann::json& record);

LegacyInvInfo queryLegacyInvInfo(const nlohmann::json& record);
