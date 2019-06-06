/*
// Copyright (c) 2018 Intel Corporation
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

#pragma once

#include <systemd/sd-journal.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

inline void logDeviceAdded(const nlohmann::json& record)
{

    auto findType = record.find("Type");
    auto findAsset =
        record.find("xyz.openbmc_project.Inventory.Decorator.Asset");

    std::string model = "Unkown";
    std::string type = "Unknown";
    std::string sn = "Unkown";

    if (findType != record.end())
    {
        type = findType->get<std::string>();
    }
    if (findAsset != record.end())
    {
        auto findModel = findAsset->find("Model");
        auto findSn = findAsset->find("SerialNumber");
        if (findModel != findAsset->end())
        {
            model = findModel->get<std::string>();
        }
        if (findSn != findAsset->end())
        {
            sn = findSn->get<std::string>();
        }
    }

    sd_journal_send("MESSAGE=%s", "Inventory Added", "PRIORITY=%i", LOG_ERR,
                    "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryAdded",
                    "REDFISH_MESSAGE_ARGS=%s,%s,%s", model.c_str(),
                    type.c_str(), sn.c_str(), NULL);
}

inline void logDeviceRemoved(const nlohmann::json& record)
{
    auto findType = record.find("Type");
    auto findAsset =
        record.find("xyz.openbmc_project.Inventory.Decorator.Asset");

    std::string model = "Unkown";
    std::string type = "Unknown";
    std::string sn = "Unkown";

    if (findType != record.end())
    {
        type = findType->get<std::string>();
    }
    if (findAsset != record.end())
    {
        auto findModel = findAsset->find("Model");
        auto findSn = findAsset->find("SerialNumber");
        if (findModel != findAsset->end())
        {
            model = findModel->get<std::string>();
        }
        if (findSn != findAsset->end())
        {
            sn = findSn->get<std::string>();
        }
    }

    sd_journal_send("MESSAGE=%s", "Inventory Removed", "PRIORITY=%i", LOG_ERR,
                    "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryRemoved",
                    "REDFISH_MESSAGE_ARGS=%s,%s,%s", model.c_str(),
                    type.c_str(), sn.c_str(), NULL);
}

enum class TemplateOperation
{
    addition,
    division,
    multiplication,
    subtraction,
    modulo,
};