#include "log_device_inventory.hpp"

#include "../utils.hpp"

#include <systemd/sd-journal.h>

#include <nlohmann/json.hpp>

#include <string>

static void setStringIfFound(std::string& value, const std::string& key,
                             const nlohmann::json& record, bool dump = false)
{
    const nlohmann::json::const_iterator find = record.find(key);

    if (find == record.end())
    {
        return;
    }

    const std::string* foundValue = find->get_ptr<const std::string*>();
    if (foundValue != nullptr)
    {
        value = *foundValue;
    }
    else if (dump)
    {
        value = find->dump();
    }
}

InvAddRemoveInfo queryInvInfo(const nlohmann::json& record)
{
    InvAddRemoveInfo ret;

    setStringIfFound(ret.type, "Type", record);
    setStringIfFound(ret.name, "Name", record);

    const nlohmann::json::const_iterator findAsset =
        record.find("xyz.openbmc_project.Inventory.Decorator.Asset");

    if (findAsset != record.end())
    {
        setStringIfFound(ret.model, "Model", *findAsset);
        setStringIfFound(ret.sn, "SerialNumber", *findAsset, true);
    }

    return ret;
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

    const InvAddRemoveInfo info = queryInvInfo(record);

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

    const InvAddRemoveInfo info = queryInvInfo(record);

    sd_journal_send(
        "MESSAGE=Inventory Removed: %s", info.name.c_str(), "PRIORITY=%i",
        LOG_INFO, "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryRemoved",
        "REDFISH_MESSAGE_ARGS=%s,%s,%s", info.model.c_str(), info.type.c_str(),
        info.sn.c_str(), "NAME=%s", info.name.c_str(), NULL);
}
