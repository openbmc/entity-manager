// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once

#include <nlohmann/json.hpp>

#include <string>

// @brief Model/Type/SerialNumber for the legacy OpenBMC.0.1.* journal
//        message, kept alongside the structured D-Bus event as a temporary
//        compatibility shim.
struct LegacyInvInfo
{
    std::string model = "Unknown";
    std::string type = "Unknown";
    std::string sn = "Unknown";
};

void logDeviceAdded(const nlohmann::json& record);

void logDeviceRemoved(const nlohmann::json& record);

std::string queryInvName(const nlohmann::json& record);

LegacyInvInfo queryLegacyInvInfo(const nlohmann::json& record);
