
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

TEST(LogDevicInventory, RecordsToLogAddedEmptyBaselineReturnsAll)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "boardA": {"Name": "Board A", "Type": "Board"},
    "boardB": {"Name": "Board B", "Type": "Board"}
}
    )");

    std::vector<const nlohmann::json*> records =
        recordsToLogAdded(newConfiguration, {});

    ASSERT_EQ(records.size(), 2U);
    EXPECT_EQ(queryInvName(*records[0]), "Board A");
    EXPECT_EQ(queryInvName(*records[1]), "Board B");
}

TEST(LogDevicInventory, RecordsToLogAddedSkipsBaselineKeys)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "boardA": {"Name": "Board A", "Type": "Board"},
    "boardB": {"Name": "Board B", "Type": "Board"}
}
    )");

    std::vector<const nlohmann::json*> records =
        recordsToLogAdded(newConfiguration, {"boardA"});

    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(queryInvName(*records[0]), "Board B");
}

TEST(LogDevicInventory, RecordsToLogAddedReturnsRecordOnceRemovedFromBaseline)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "boardA": {"Name": "Board A", "Type": "Board"}
}
    )");
    std::unordered_set<std::string> cachedBaseline = {"boardA"};

    EXPECT_TRUE(recordsToLogAdded(newConfiguration, cachedBaseline).empty());

    // An InventoryRemoved was reported for boardA, so it is new again.
    cachedBaseline.erase("boardA");

    std::vector<const nlohmann::json*> records =
        recordsToLogAdded(newConfiguration, cachedBaseline);

    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(queryInvName(*records[0]), "Board A");
}

TEST(LogDevicInventory, RecordsToLogAddedIgnoresBaselineKeysNotPresent)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "boardA": {"Name": "Board A", "Type": "Board"}
}
    )");

    std::vector<const nlohmann::json*> records =
        recordsToLogAdded(newConfiguration, {"boardGone"});

    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(queryInvName(*records[0]), "Board A");
}

TEST(LogDevicInventory, RecordsToLogAddedNothingNew)
{
    EXPECT_TRUE(recordsToLogAdded(nlohmann::json::object(), {}).empty());
    EXPECT_TRUE(recordsToLogAdded(nlohmann::json(), {"boardA"}).empty());
}
