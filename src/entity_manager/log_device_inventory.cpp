#include "../utils.hpp"

#include <systemd/sd-journal.h>

#include <nlohmann/json.hpp>

#include <flat_map>
#include <string>

struct InvAddRemoveInfo
{
    std::string model = "Unknown";
    std::string type = "Unknown";
    std::string sn = "Unknown";
    std::string name = "Unknown";
};

static InvAddRemoveInfo queryInvInfo(const nlohmann::json& record)
{
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

    return {model, type, sn, name};
}

void logDeviceAdded(const nlohmann::json& record)
{
    if (!EM_CACHE_CONFIGURATION)
    {
        return;
    }
    if (!deviceHasLogging(record))
    {
        return;
    }

    const auto info = queryInvInfo(record);

    sd_journal_send(
        "MESSAGE=Inventory Added: %s", info.name.c_str(), "PRIORITY=%i",
        LOG_INFO, "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryAdded",
        "REDFISH_MESSAGE_ARGS=%s,%s,%s", info.model.c_str(), info.type.c_str(),
        info.sn.c_str(), "NAME=%s", info.name.c_str(), NULL);
}

void logDeviceRemoved(const nlohmann::json& record)
{
    if (!deviceHasLogging(record))
    {
        return;
    }

    const auto info = queryInvInfo(record);

    sd_journal_send(
        "MESSAGE=Inventory Removed: %s", info.name.c_str(), "PRIORITY=%i",
        LOG_INFO, "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryRemoved",
        "REDFISH_MESSAGE_ARGS=%s,%s,%s", info.model.c_str(), info.type.c_str(),
        info.sn.c_str(), "NAME=%s", info.name.c_str(), NULL);
}
