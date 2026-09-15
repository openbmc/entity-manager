#include "log_device_inventory.hpp"

#include "../dbus_util.hpp"
#include "utils.hpp"

#include <systemd/sd-journal.h>

#include <nlohmann/json.hpp>
#include <phosphor-logging/commit.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/common.hpp>
#include <xyz/openbmc_project/Inventory/event.hpp>

#include <flat_map>
#include <string>

namespace inventory_event = sdbusplus::event::xyz::openbmc_project::Inventory;

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

    const nlohmann::json::const_iterator findAsset = record.find(
        sdbusplus::common::xyz::openbmc_project::inventory::decorator::Asset::
            interface);

    if (findAsset != record.end())
    {
        setStringIfFound(ret.model, "Model", *findAsset);
        setStringIfFound(ret.sn, "SerialNumber", *findAsset, true);
    }

    return ret;
}

std::optional<sdbusplus::object_path> inventoryPath(
    const nlohmann::json& record)
{
    const nlohmann::json::const_iterator findName = record.find("Name");
    if (findName == record.end())
    {
        return std::nullopt;
    }

    const std::string* namePtr = findName->get_ptr<const std::string*>();
    if (namePtr == nullptr)
    {
        return std::nullopt;
    }

    // postBoardToDBus() falls back to 'Chassis' when no type is given
    std::string type = "Chassis";
    setStringIfFound(type, "Type", record);

    std::string name = *namePtr;

    return em_utils::buildInventorySystemPath(
        name, dbus_util::sanitizeForDBusPathSegment(type));
}

// @brief        commit an Inventory{Added,Removed} event to phosphor-logging
// @param added  true for InventoryAdded, false for InventoryRemoved
// @param record the configuration record of the device
static void commitInventoryEvent(bool added, const nlohmann::json& record)
{
    const std::optional<sdbusplus::object_path> path = inventoryPath(record);
    if (!path.has_value())
    {
        lg2::warning(
            "Not logging inventory event, no name found in configuration");
        return;
    }

    try
    {
        if (added)
        {
            lg2::commit(
                inventory_event::InventoryAdded("IDENTIFIER_PATH", *path));
        }
        else
        {
            lg2::commit(
                inventory_event::InventoryRemoved("IDENTIFIER_PATH", *path));
        }
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to commit inventory event for {PATH}: {ERR}", "PATH",
                   path->str, "ERR", e.what());
    }
}

void logDeviceAdded(const nlohmann::json& record)
{
    if (!EM_CACHE_CONFIGURATION)
    {
        return;
    }

    const InvAddRemoveInfo info = queryInvInfo(record);

    sd_journal_send(
        "MESSAGE=Inventory Added: %s", info.name.c_str(), "PRIORITY=%i",
        LOG_INFO, "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryAdded",
        "REDFISH_MESSAGE_ARGS=%s,%s,%s", info.model.c_str(), info.type.c_str(),
        info.sn.c_str(), "NAME=%s", info.name.c_str(), NULL);

    commitInventoryEvent(true, record);
}

void logDeviceRemoved(const nlohmann::json& record)
{
    const InvAddRemoveInfo info = queryInvInfo(record);

    sd_journal_send(
        "MESSAGE=Inventory Removed: %s", info.name.c_str(), "PRIORITY=%i",
        LOG_INFO, "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryRemoved",
        "REDFISH_MESSAGE_ARGS=%s,%s,%s", info.model.c_str(), info.type.c_str(),
        info.sn.c_str(), "NAME=%s", info.name.c_str(), NULL);

    commitInventoryEvent(false, record);
}
