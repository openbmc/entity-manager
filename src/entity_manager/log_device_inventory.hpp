// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once

#include "em_config.hpp"

#include <nlohmann/json.hpp>

struct InvAddRemoveInfo
{
    std::string model = "Unknown";
    std::string type = "Unknown";
    std::string sn = "Unknown";
    std::string name = "Unknown";
};

void logDeviceAdded(const EMConfig& record);

void logDeviceRemoved(const EMConfig& record);

InvAddRemoveInfo queryInvInfo(const EMConfig& record);
