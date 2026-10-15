// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once

#include "em_config.hpp"

#include <string>
#include <unordered_set>

// @brief Model/Type/SerialNumber for the legacy OpenBMC.0.1.* journal
//        message, kept alongside the structured D-Bus event as a temporary
//        compatibility shim.
struct LegacyInvInfo
{
    std::string model = "Unknown";
    std::string type = "Unknown";
    std::string sn = "Unknown";
};

void logDeviceAdded(const EMConfig& record);

void logDeviceRemoved(const std::string& key, const EMConfig& record,
                      std::unordered_set<std::string>& cachedBaseline);

LegacyInvInfo queryLegacyInvInfo(const EMConfig& record);

// Compatibility entry points for live configurations still stored as JSON.
void logDeviceAdded(const nlohmann::json& record);
void logDeviceRemoved(const std::string& key, const nlohmann::json& record,
                      std::unordered_set<std::string>& cachedBaseline);
std::string queryInvName(const nlohmann::json& record);
LegacyInvInfo queryLegacyInvInfo(const nlohmann::json& record);
