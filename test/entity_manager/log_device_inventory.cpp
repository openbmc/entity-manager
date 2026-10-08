
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

    LegacyInvInfo info = queryLegacyInvInfo(record);

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

    LegacyInvInfo info = queryLegacyInvInfo(record);

    EXPECT_EQ(info.type, "PowerSupply");
    EXPECT_EQ(info.sn, "43829239");
    EXPECT_EQ(info.model, "Unknown");
}

TEST(LogDevicInventory, KeysToLogAddedEmptyBaselineReturnsAll)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "boardA": {"Name": "Board A", "Type": "Board"},
    "boardB": {"Name": "Board B", "Type": "Board"}
}
    )");

    EXPECT_EQ(keysToLogAdded(newConfiguration, {}),
              (std::vector<std::string>{"boardA", "boardB"}));
}

TEST(LogDevicInventory, KeysToLogAddedSkipsBaselineKeys)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "boardA": {"Name": "Board A", "Type": "Board"},
    "boardB": {"Name": "Board B", "Type": "Board"}
}
    )");

    EXPECT_EQ(keysToLogAdded(newConfiguration, {"boardA"}),
              (std::vector<std::string>{"boardB"}));
}

TEST(LogDevicInventory, KeysToLogAddedReturnsKeyOnceRemovedFromBaseline)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "boardA": {"Name": "Board A", "Type": "Board"}
}
    )");
    std::unordered_set<std::string> cachedBaseline = {"boardA"};

    EXPECT_TRUE(keysToLogAdded(newConfiguration, cachedBaseline).empty());

    // An InventoryRemoved was reported for boardA, so it is new again.
    cachedBaseline.erase("boardA");

    EXPECT_EQ(keysToLogAdded(newConfiguration, cachedBaseline),
              (std::vector<std::string>{"boardA"}));
}

TEST(LogDevicInventory, KeysToLogAddedIgnoresBaselineKeysNotPresent)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "boardA": {"Name": "Board A", "Type": "Board"}
}
    )");

    EXPECT_EQ(keysToLogAdded(newConfiguration, {"boardGone"}),
              (std::vector<std::string>{"boardA"}));
}

TEST(LogDevicInventory, KeysToLogAddedNothingNew)
{
    EXPECT_TRUE(keysToLogAdded(nlohmann::json::object(), {}).empty());
    EXPECT_TRUE(keysToLogAdded(nlohmann::json(), {"boardA"}).empty());
}
