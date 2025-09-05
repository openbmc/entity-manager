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
/// \file entity_manager.hpp

#pragma once

#include "utils.hpp"

#include <systemd/sd-journal.h>

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>
#include <list>
#include <optional>
#include <string>

struct DBusDeviceDescriptor
{
    DBusInterface interface;
    std::string path;
};

using FoundDevices = std::vector<DBusDeviceDescriptor>;

struct CmpStr
{
    bool operator()(const char* a, const char* b) const
    {
        return std::strcmp(a, b) < 0;
    }
};

// underscore T for collison with dbus c api
enum class probe_type_codes
{
    FALSE_T,
    TRUE_T,
    AND,
    OR,
    JOINT,
    FOUND,
    MATCH_ONE
};

using FoundProbeTypeT =
    std::optional<boost::container::flat_map<const char*, probe_type_codes,
                                             CmpStr>::const_iterator>;
FoundProbeTypeT findProbeType(const std::string& probe);

struct PerformScan : std::enable_shared_from_this<PerformScan>
{
    PerformScan(nlohmann::json& systemConfiguration,
                nlohmann::json& missingConfigurations,
                std::list<nlohmann::json>& configurations,
                sdbusplus::asio::object_server& objServer,
                std::function<void()>&& callback);
    void updateSystemConfiguration(const nlohmann::json& recordRef,
                                   const std::string& probeName,
                                   FoundDevices& foundDevices);
    void run(void);
    virtual ~PerformScan();
    nlohmann::json& _systemConfiguration;
    nlohmann::json& _missingConfigurations;
    std::list<nlohmann::json> _configurations;
    sdbusplus::asio::object_server& objServer;
    std::function<void()> _callback;
    bool _passed = false;
    MapperGetSubTreeResponse dbusProbeObjects;
    std::vector<std::string> passedProbes;
};

// this class finds the needed dbus fields and on destruction runs the probe
struct PerformProbe : std::enable_shared_from_this<PerformProbe>
{
    PerformProbe(nlohmann::json& recordRef,
                 const std::vector<std::string>& probeCommand,
                 std::string probeName, std::shared_ptr<PerformScan>& scanPtr);
    virtual ~PerformProbe();

    nlohmann::json& recordRef;
    std::vector<std::string> _probeCommand;
    std::string probeName;
    std::shared_ptr<PerformScan> scan;
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

    std::string model = "Unknown";
    std::string type = "Unknown";
    std::string sn = "Unknown";
    std::string name = "Unknown";

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

    auto findName = record.find("Name");
    if (findName != record.end())
    {
        name = findName->get<std::string>();
    }

    sd_journal_send("MESSAGE=Inventory Added: %s", name.c_str(), "PRIORITY=%i",
                    LOG_INFO, "REDFISH_MESSAGE_ID=%s",
                    "OpenBMC.0.1.InventoryAdded",
                    "REDFISH_MESSAGE_ARGS=%s,%s,%s", model.c_str(),
                    type.c_str(), sn.c_str(), "NAME=%s", name.c_str(), NULL);
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

    std::string model = "Unknown";
    std::string type = "Unknown";
    std::string sn = "Unknown";
    std::string name = "Unknown";

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

    auto findName = record.find("Name");
    if (findName != record.end())
    {
        name = findName->get<std::string>();
    }

    sd_journal_send("MESSAGE=Inventory Removed: %s", name.c_str(),
                    "PRIORITY=%i", LOG_INFO, "REDFISH_MESSAGE_ID=%s",
                    "OpenBMC.0.1.InventoryRemoved",
                    "REDFISH_MESSAGE_ARGS=%s,%s,%s", model.c_str(),
                    type.c_str(), sn.c_str(), "NAME=%s", name.c_str(), NULL);
}

// find the intersection of two sets of devices based on the object path, return
// the result in set1. set2 is destroyed.
// Note: if there are duplicates in either set or both, the duplicates will be
// kept if they are in the intersection.
inline void intersectDevices(FoundDevices& set1, FoundDevices& set2)
{
    for (auto itrDev = set2.begin(); itrDev != set2.end();)
    {
        auto findIt = std::find_if(set1.begin(), set1.end(),
                                   [&itrDev](const auto& existingDev) {
            return existingDev.path == itrDev->path;
        });
        if (findIt == set1.end())
        {
            itrDev = set2.erase(itrDev);
        }
        else
        {
            ++itrDev;
        }
    }

    for (auto itrDev = set1.begin(); itrDev != set1.end();)
    {
        auto findIt = std::find_if(set2.begin(), set2.end(),
                                   [&itrDev](const auto& existingDev) {
            return existingDev.path == itrDev->path;
        });
        if (findIt == set2.end())
        {
            itrDev = set1.erase(itrDev);
        }
        else
        {
            ++itrDev;
        }
    }
    set1.insert(set1.end(), set2.begin(), set2.end());
}
