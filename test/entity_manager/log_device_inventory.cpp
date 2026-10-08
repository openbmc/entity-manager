
#include "entity_manager/log_device_inventory.hpp"

#include <string>
#include <unordered_set>

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
    "t1Slot1": {
        "Name": "Yosemite 4 Sentinel Dome T1 Slot 1",
        "Type": "Board"
    },
    "t1Slot2": {
        "Name": "Yosemite 4 Sentinel Dome T1 Slot 2",
        "Type": "Board"
    }
}
    )");

    EXPECT_EQ(recordsToLogAdded(newConfiguration, {}), newConfiguration);
}

TEST(LogDevicInventory, RecordsToLogAddedSkipsBaselineKeys)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "t1Slot1": {
        "Name": "Yosemite 4 Sentinel Dome T1 Slot 1",
        "Type": "Board"
    },
    "t1Slot2": {
        "Name": "Yosemite 4 Sentinel Dome T1 Slot 2",
        "Type": "Board"
    }
}
    )");
    nlohmann::json expected = nlohmann::json::parse(R"(
{
    "t1Slot2": {
        "Name": "Yosemite 4 Sentinel Dome T1 Slot 2",
        "Type": "Board"
    }
}
    )");

    EXPECT_EQ(recordsToLogAdded(newConfiguration, {"t1Slot1"}), expected);
}

TEST(LogDevicInventory, RecordsToLogAddedReturnsRecordOnceRemovedFromBaseline)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "t1Slot1": {
        "Name": "Yosemite 4 Sentinel Dome T1 Slot 1",
        "Type": "Board"
    }
}
    )");
    std::unordered_set<std::string> cachedBaseline = {"t1Slot1"};

    EXPECT_TRUE(recordsToLogAdded(newConfiguration, cachedBaseline).empty());

    // An InventoryRemoved was reported for the board, so it is new again.
    cachedBaseline.erase("t1Slot1");

    EXPECT_EQ(recordsToLogAdded(newConfiguration, cachedBaseline),
              newConfiguration);
}

TEST(LogDevicInventory, RecordsToLogAddedIgnoresBaselineKeysNotPresent)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "t1Slot1": {
        "Name": "Yosemite 4 Sentinel Dome T1 Slot 1",
        "Type": "Board"
    }
}
    )");

    // The slot 2 board is cached but not part of this scan.
    EXPECT_EQ(recordsToLogAdded(newConfiguration, {"t1Slot2"}),
              newConfiguration);
}

TEST(LogDevicInventory, RecordsToLogAddedNothingNew)
{
    EXPECT_TRUE(recordsToLogAdded(nlohmann::json::object(), {}).empty());
    EXPECT_TRUE(recordsToLogAdded(nlohmann::json(), {"t1Slot1"}).empty());
}
