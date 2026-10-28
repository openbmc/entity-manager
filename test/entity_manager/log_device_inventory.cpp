
#include "entity_manager/log_device_inventory.hpp"

#include <gtest/gtest.h>

TEST(LogDeviceInventoryJson, PreservesMissingFieldDefaults)
{
    const nlohmann::json record = nlohmann::json::object();

    EXPECT_EQ(queryInvName(record), "Unknown");
    const LegacyInvInfo info = queryLegacyInvInfo(record);
    EXPECT_EQ(info.type, "Unknown");
    EXPECT_EQ(info.model, "Unknown");
    EXPECT_EQ(info.sn, "Unknown");
}

TEST(LogDeviceInventoryJson, PreservesAssetFields)
{
    const nlohmann::json record = {
        {"Name", "PSU"},
        {"Type", "PowerSupply"},
        {"xyz.openbmc_project.Inventory.Decorator.Asset",
         {{"Model", "Model"}, {"SerialNumber", 42}}}};

    EXPECT_EQ(queryInvName(record), "PSU");
    const LegacyInvInfo info = queryLegacyInvInfo(record);
    EXPECT_EQ(info.type, "PowerSupply");
    EXPECT_EQ(info.model, "Model");
    EXPECT_EQ(info.sn, "42");
}

TEST(LogDeviceInventory, PreservesMissingAssetDefaults)
{
    EMConfig config;
    config.type = "PowerSupply";

    const LegacyInvInfo info = queryLegacyInvInfo(config);

    EXPECT_EQ(info.type, "PowerSupply");
    EXPECT_EQ(info.model, "Unknown");
    EXPECT_EQ(info.sn, "Unknown");
}

TEST(LogDeviceInventory, PreservesNumericSerialNumber)
{
    EMConfig config;
    config.type = "PowerSupply";
    config.extraInterfaces["xyz.openbmc_project.Inventory.Decorator.Asset"] = {
        {"Model", "Model"}, {"SerialNumber", 42}};

    const LegacyInvInfo info = queryLegacyInvInfo(config);

    EXPECT_EQ(info.model, "Model");
    EXPECT_EQ(info.sn, "42");
}

TEST(LogDevicInventory, QueryLegacyInvInfoSuccess)
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

    const auto config = EMConfig::fromJson(record);
    ASSERT_TRUE(config.has_value());
    LegacyInvInfo info = queryLegacyInvInfo(config.value());

    EXPECT_EQ(config.value().name, "Supermicro PWS 920P SQ 0");
    EXPECT_EQ(info.type, "PowerSupply");
    EXPECT_EQ(info.sn, "43829239");
    EXPECT_EQ(info.model, "PWS 920P SQ");
}

TEST(LogDevicInventory, QueryLegacyInvInfoNoModelFound)
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

    const auto config = EMConfig::fromJson(record);
    ASSERT_TRUE(config.has_value());
    LegacyInvInfo info = queryLegacyInvInfo(config.value());

    EXPECT_EQ(info.type, "PowerSupply");
    EXPECT_EQ(info.sn, "43829239");
    EXPECT_EQ(info.model, "Unknown");
}
