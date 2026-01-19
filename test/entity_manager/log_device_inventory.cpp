
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

    const auto optConfig = EMConfig::fromJson(record);

    ASSERT_TRUE(optConfig.has_value());

    if (optConfig.has_value())
    {
        InvAddRemoveInfo info = queryInvInfo(optConfig.value());

        EXPECT_EQ(info.name, "Supermicro PWS 920P SQ 0");
        EXPECT_EQ(info.type, "PowerSupply");
        EXPECT_EQ(info.sn, "43829239");
        EXPECT_EQ(info.model, "PWS 920P SQ");
    }
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

    const auto optConfig = EMConfig::fromJson(record);

    ASSERT_TRUE(optConfig.has_value());

    if (optConfig.has_value())
    {
        InvAddRemoveInfo info = queryInvInfo(optConfig.value());

        EXPECT_EQ(info.name, "Supermicro PWS 920P SQ 0");
        EXPECT_EQ(info.type, "PowerSupply");
        EXPECT_EQ(info.sn, "43829239");
        EXPECT_EQ(info.model, "Unknown");
    }
}
