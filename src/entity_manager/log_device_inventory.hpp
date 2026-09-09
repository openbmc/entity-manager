// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once

#include <nlohmann/json.hpp>

#include <string>

void logDeviceAdded(const nlohmann::json& record);

void logDeviceRemoved(const nlohmann::json& record);

std::string queryInvName(const nlohmann::json& record);
