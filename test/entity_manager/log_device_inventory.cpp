
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
    "chassis": {
        "Name": "Yosemite 4 Sentinel Dome Slot 1 Chassis",
        "Type": "Chassis"
    },
    "cpu": {
        "Name": "Yosemite 4 Sentinel Dome Slot 1 CPU",
        "Type": "Cpu"
    }
}
    )");

    EXPECT_EQ(recordsToLogAdded(newConfiguration, {}), newConfiguration);
}

TEST(LogDevicInventory, RecordsToLogAddedSkipsBaselineKeys)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "chassis": {
        "Name": "Yosemite 4 Sentinel Dome Slot 1 Chassis",
        "Type": "Chassis"
    },
    "cpu": {
        "Name": "Yosemite 4 Sentinel Dome Slot 1 CPU",
        "Type": "Cpu"
    }
}
    )");
    nlohmann::json expected = nlohmann::json::parse(R"(
{
    "cpu": {
        "Name": "Yosemite 4 Sentinel Dome Slot 1 CPU",
        "Type": "Cpu"
    }
}
    )");

    EXPECT_EQ(recordsToLogAdded(newConfiguration, {"chassis"}), expected);
}

TEST(LogDevicInventory, RecordsToLogAddedReturnsRecordOnceRemovedFromBaseline)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "cpu": {
        "Name": "Yosemite 4 Sentinel Dome Slot 1 CPU",
        "Type": "Cpu"
    }
}
    )");
    std::unordered_set<std::string> cachedBaseline = {"cpu"};

    EXPECT_TRUE(recordsToLogAdded(newConfiguration, cachedBaseline).empty());

    // An InventoryRemoved was reported for the CPU, so it is new again.
    cachedBaseline.erase("cpu");

    EXPECT_EQ(recordsToLogAdded(newConfiguration, cachedBaseline),
              newConfiguration);
}

TEST(LogDevicInventory, RecordsToLogAddedIgnoresBaselineKeysNotPresent)
{
    nlohmann::json newConfiguration = nlohmann::json::parse(R"(
{
    "cpu": {
        "Name": "Yosemite 4 Sentinel Dome Slot 1 CPU",
        "Type": "Cpu"
    }
}
    )");

    // The chassis is cached but not part of this scan.
    EXPECT_EQ(recordsToLogAdded(newConfiguration, {"chassis"}),
              newConfiguration);
}

TEST(LogDevicInventory, RecordsToLogAddedNothingNew)
{
    EXPECT_TRUE(recordsToLogAdded(nlohmann::json::object(), {}).empty());
    EXPECT_TRUE(recordsToLogAdded(nlohmann::json(), {"cpu"}).empty());
}
