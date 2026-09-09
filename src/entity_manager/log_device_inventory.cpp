#include "log_device_inventory.hpp"

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

    setStringIfFound(ret.name, "Name", record);

    return ret;
}

static std::optional<sdbusplus::object_path> inventoryPath(
    const nlohmann::json& record, const InvAddRemoveInfo& info)
{
    std::string boardName = info.name;
    std::optional<std::string> boardType =
        em_utils::resolveConfigType(record, info.name);
    if (!boardType)
    {
        return std::nullopt;
    }

    return em_utils::buildInventorySystemPath(boardName, *boardType);
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
    std::optional<sdbusplus::object_path> path = inventoryPath(record, info);
    if (!path)
    {
        return;
    }
    lg2::commit(InventoryAdded("IDENTIFIER_PATH", *path));
}

void logDeviceRemoved(const nlohmann::json& record)
{
    using InventoryRemoved =
        sdbusplus::event::xyz::openbmc_project::Inventory::InventoryRemoved;

    const InvAddRemoveInfo info = queryInvInfo(record);
    std::optional<sdbusplus::object_path> path = inventoryPath(record, info);
    if (!path)
    {
        return;
    }
    lg2::commit(InventoryRemoved("IDENTIFIER_PATH", *path));
}
