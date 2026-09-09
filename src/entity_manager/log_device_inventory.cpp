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

std::string queryInvName(const nlohmann::json& record)
{
    std::string name = "Unknown";

    setStringIfFound(name, "Name", record);

    return name;
}

static std::optional<sdbusplus::object_path> inventoryPath(
    const nlohmann::json& record, const std::string& name)
{
    std::optional<std::string> boardType =
        em_utils::resolveConfigType(record, name);
    if (!boardType)
    {
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

    using InventoryAdded =
        sdbusplus::event::xyz::openbmc_project::Inventory::InventoryAdded;

    const std::string name = queryInvName(record);
    std::optional<sdbusplus::object_path> path = inventoryPath(record, name);
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

    const std::string name = queryInvName(record);
    std::optional<sdbusplus::object_path> path = inventoryPath(record, name);
    if (!path)
    {
        return;
    }
    lg2::commit(InventoryRemoved("IDENTIFIER_PATH", *path));
}
