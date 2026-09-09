#include "log_device_inventory.hpp"

#include "../dbus_util.hpp"
#include "utils.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/commit.hpp>
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

InvAddRemoveInfo queryInvInfo(const nlohmann::json& record)
{
    InvAddRemoveInfo ret;

    setStringIfFound(ret.type, "Type", record);
    setStringIfFound(ret.name, "Name", record);

    return ret;
}

static sdbusplus::object_path inventoryPath(const InvAddRemoveInfo& info)
{
    std::string boardName = info.name;
    std::string boardType = dbus_util::sanitizeForDBusPathSegment(info.type);

    return em_utils::buildInventorySystemPath(boardName, boardType);
}

void logDeviceAdded(const nlohmann::json& record)
{
    if (!EM_CACHE_CONFIGURATION)
    {
        return;
    }

    using InventoryAdded =
        sdbusplus::event::xyz::openbmc_project::Inventory::InventoryAdded;

    const InvAddRemoveInfo info = queryInvInfo(record);
    lg2::commit(InventoryAdded("IDENTIFIER_PATH", inventoryPath(info)));
}

void logDeviceRemoved(const nlohmann::json& record)
{
    using InventoryRemoved =
        sdbusplus::event::xyz::openbmc_project::Inventory::InventoryRemoved;

    const InvAddRemoveInfo info = queryInvInfo(record);
    lg2::commit(InventoryRemoved("IDENTIFIER_PATH", inventoryPath(info)));
}
