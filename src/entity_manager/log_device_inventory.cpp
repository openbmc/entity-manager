#include "log_device_inventory.hpp"

#include "utils.hpp"

#include <systemd/sd-journal.h>

#include <nlohmann/json.hpp>
#include <phosphor-logging/commit.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/common.hpp>
#include <xyz/openbmc_project/Inventory/event.hpp>

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

std::string queryInvName(const nlohmann::json& record)
{
    std::string name = "Unknown";

    setStringIfFound(name, "Name", record);

    return name;
}

LegacyInvInfo queryLegacyInvInfo(const nlohmann::json& record)
{
    LegacyInvInfo ret;

    setStringIfFound(ret.type, "Type", record);

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

static std::optional<sdbusplus::object_path> inventoryPath(
    const nlohmann::json& record, const std::string& name)
{
    std::optional<std::string> boardType = em_utils::resolveConfigType(record);
    if (!boardType)
    {
        lg2::error("Type for {CONFIG} was missing or not a string, not "
                   "logging inventory event",
                   "CONFIG", name);
        return std::nullopt;
    }

    std::string boardName = name;
    return em_utils::buildInventorySystemPath(boardName, *boardType);
}

void logDeviceAdded(const nlohmann::json& record)
{
    if (!EM_CACHE_CONFIGURATION)
    {
        return;
    }

    const std::string name = queryInvName(record);

    // Temporary compatibility shim: bmcweb's default journal-based EventLog
    // backend does not yet understand lg2::commit()'s D-Bus event, so keep
    // emitting the legacy message until that support exists upstream.
    const LegacyInvInfo info = queryLegacyInvInfo(record);
    sd_journal_send(
        "MESSAGE=Inventory Added: %s", name.c_str(), "PRIORITY=%i", LOG_INFO,
        "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryAdded",
        "REDFISH_MESSAGE_ARGS=%s,%s,%s", info.model.c_str(), info.type.c_str(),
        info.sn.c_str(), "NAME=%s", name.c_str(), NULL);

    using InventoryAdded =
        sdbusplus::event::xyz::openbmc_project::Inventory::InventoryAdded;

    std::optional<sdbusplus::object_path> path = inventoryPath(record, name);
    if (!path)
    {
        return;
    }
    try
    {
        lg2::commit(InventoryAdded("IDENTIFIER_PATH", *path));
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to commit InventoryAdded for {PATH}: {ERR}", "PATH",
                   *path, "ERR", e);
    }
}

void logDeviceRemoved(const nlohmann::json& record)
{
    const std::string name = queryInvName(record);

    // Temporary compatibility shim: see logDeviceAdded().
    const LegacyInvInfo info = queryLegacyInvInfo(record);
    sd_journal_send(
        "MESSAGE=Inventory Removed: %s", name.c_str(), "PRIORITY=%i", LOG_INFO,
        "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryRemoved",
        "REDFISH_MESSAGE_ARGS=%s,%s,%s", info.model.c_str(), info.type.c_str(),
        info.sn.c_str(), "NAME=%s", name.c_str(), NULL);

    using InventoryRemoved =
        sdbusplus::event::xyz::openbmc_project::Inventory::InventoryRemoved;

    std::optional<sdbusplus::object_path> path = inventoryPath(record, name);
    if (!path)
    {
        return;
    }
    try
    {
        lg2::commit(InventoryRemoved("IDENTIFIER_PATH", *path));
    }
    catch (const std::exception& e)
    {
        lg2::error("Failed to commit InventoryRemoved for {PATH}: {ERR}",
                   "PATH", *path, "ERR", e);
    }
}
