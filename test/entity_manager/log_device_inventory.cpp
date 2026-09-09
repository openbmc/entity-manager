
#include "entity_manager/log_device_inventory.hpp"

#include <gtest/gtest.h>

TEST(LogDevicInventory, QueryInvNameSuccess)
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

    std::string name = queryInvName(record);

    EXPECT_EQ(name, "Supermicro PWS 920P SQ 0");
}

TEST(LogDevicInventory, QueryInvNameNoNameFound)
{
    nlohmann::json record = nlohmann::json::parse(R"(
{
    "Exposes": [],
    "Probe": "TRUE",
    "Type": "PowerSupply"
}
    )");

    std::string name = queryInvName(record);

    EXPECT_EQ(name, "Unknown");
}
