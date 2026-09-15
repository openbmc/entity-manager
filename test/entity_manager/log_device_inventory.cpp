
#include "entity_manager/log_device_inventory.hpp"

#include <gtest/gtest.h>

TEST(LogDevicInventory, QueryInvInfoSuccess)
{
    nlohmann::json record = nlohmann::json::parse(R"(
{
    "Exposes": [],
    "Name": "Supermicro PWS 920P SQ 0",
    "Probe": "TRUE",
    "Type": "PowerSupply",
    "xyz.openbmc_project.Inventory.Decorator.Asset": {
        "Manufacturer": "Supermicro",
        "Model": "PWS 920P SQ",
        "PartNumber": "328923",
        "SerialNumber": "43829239"
    }
}
   )");

    InvAddRemoveInfo info = queryInvInfo(record);

    EXPECT_EQ(info.name, "Supermicro PWS 920P SQ 0");
    EXPECT_EQ(info.type, "PowerSupply");
    EXPECT_EQ(info.sn, "43829239");
    EXPECT_EQ(info.model, "PWS 920P SQ");
}

TEST(LogDevicInventory, QueryInvInfoNoModelFound)
{
    nlohmann::json record = nlohmann::json::parse(R"(
{
    "Exposes": [],
    "Name": "Supermicro PWS 920P SQ 0",
    "Probe": "TRUE",
    "Type": "PowerSupply",
    "xyz.openbmc_project.Inventory.Decorator.Asset": {
        "Manufacturer": "Supermicro",
        "PartNumber": "328923",
        "SerialNumber": "43829239"
    }
}
    )");

    InvAddRemoveInfo info = queryInvInfo(record);

    EXPECT_EQ(info.name, "Supermicro PWS 920P SQ 0");
    EXPECT_EQ(info.type, "PowerSupply");
    EXPECT_EQ(info.sn, "43829239");
    EXPECT_EQ(info.model, "Unknown");
}

TEST(LogDevicInventory, InventoryPathSuccess)
{
    nlohmann::json record = nlohmann::json::parse(R"(
{
    "Name": "Supermicro PWS 920P SQ 0",
    "Type": "PowerSupply"
}
    )");

    std::optional<sdbusplus::object_path> path = inventoryPath(record);

    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->str, "/xyz/openbmc_project/inventory/system/powersupply/"
                         "Supermicro_PWS_920P_SQ_0");
}

TEST(LogDevicInventory, InventoryPathNoTypeDefaultsToChassis)
{
    nlohmann::json record = nlohmann::json::parse(R"(
{
    "Name": "My Board"
}
    )");

    std::optional<sdbusplus::object_path> path = inventoryPath(record);

    ASSERT_TRUE(path.has_value());
    EXPECT_EQ(path->str,
              "/xyz/openbmc_project/inventory/system/chassis/My_Board");
}

TEST(LogDevicInventory, InventoryPathNoName)
{
    nlohmann::json record = nlohmann::json::parse(R"(
{
    "Type": "PowerSupply"
}
    )");

    EXPECT_FALSE(inventoryPath(record).has_value());
}
