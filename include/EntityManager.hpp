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
/// \file EntityManager.hpp

#pragma once

#include "Utils.hpp"

#include <systemd/sd-journal.h>

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>
#include <list>
#include <string>

extern std::shared_ptr<sdbusplus::asio::connection> SYSTEM_BUS;
extern boost::asio::io_context io;
extern nlohmann::json lastJson;
constexpr const char* currentConfiguration = "/var/configuration/system.json";

// paths - > interfaces -> properties
using DBusProbeObjectT = boost::container::flat_map<
    std::string,
    boost::container::flat_map<
        std::string,
        boost::container::flat_map<std::string, BasicVariantType>>>;

// vector of tuple<map<propertyName, variant>, D-Bus path>>
using FoundDeviceT = std::vector<std::tuple<
    boost::container::flat_map<std::string, BasicVariantType>, std::string>>;

struct PerformScan : std::enable_shared_from_this<PerformScan>
{

    PerformScan(nlohmann::json& systemConfiguration,
                nlohmann::json& missingConfigurations,
                std::list<nlohmann::json>& configurations,
                sdbusplus::asio::object_server& objServer,
                std::function<void()>&& callback);
    void run(void);
    virtual ~PerformScan();
    nlohmann::json& _systemConfiguration;
    nlohmann::json& _missingConfigurations;
    std::list<nlohmann::json> _configurations;
    sdbusplus::asio::object_server& objServer;
    std::function<void()> _callback;
    bool _passed = false;
    bool powerWasOn = isPowerOn();
    DBusProbeObjectT dbusProbeObjects;
    std::vector<std::string> passedProbes;
};

// this class finds the needed dbus fields and on destruction runs the probe
struct PerformProbe : std::enable_shared_from_this<PerformProbe>
{
    PerformProbe(
        const std::vector<std::string>& probeCommand,
        std::shared_ptr<PerformScan>& scanPtr,
        std::function<void(FoundDeviceT&, const DBusProbeObjectT&)>&& callback);
    virtual ~PerformProbe();

    std::vector<std::string> _probeCommand;
    std::shared_ptr<PerformScan> scan;
    std::function<void(FoundDeviceT&, const DBusProbeObjectT&)> _callback;
};

inline void logDeviceAdded(const nlohmann::json& record)
{

    if (!deviceHasLogging(record))
    {
        return;
    }
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
            const std::string* getSn = findSn->get_ptr<const std::string*>();
            if (getSn != nullptr)
            {
                sn = *getSn;
            }
            else
            {
                sn = findSn->dump();
            }
        }
    }

    sd_journal_send("MESSAGE=%s", "Inventory Added", "PRIORITY=%i", LOG_ERR,
                    "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryAdded",
                    "REDFISH_MESSAGE_ARGS=%s,%s,%s", model.c_str(),
                    type.c_str(), sn.c_str(), NULL);
}

inline void logDeviceRemoved(const nlohmann::json& record)
{
    if (!deviceHasLogging(record))
    {
        return;
    }
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
            const std::string* getSn = findSn->get_ptr<const std::string*>();
            if (getSn != nullptr)
            {
                sn = *getSn;
            }
            else
            {
                sn = findSn->dump();
            }
        }
    }

    sd_journal_send("MESSAGE=%s", "Inventory Removed", "PRIORITY=%i", LOG_ERR,
                    "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryRemoved",
                    "REDFISH_MESSAGE_ARGS=%s,%s,%s", model.c_str(),
                    type.c_str(), sn.c_str(), NULL);
}

void propertiesChangedCallback(nlohmann::json& systemConfiguration,
                               sdbusplus::asio::object_server& objServer);
