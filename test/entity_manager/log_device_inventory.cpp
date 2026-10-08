
#include "entity_manager/log_device_inventory.hpp"

#include "entity_manager/perform_scan.hpp"

#include <cstdint>
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

namespace
{

// The chassis and the CPU of Sentinel Dome Slot 1 are found through the same
// FRU device, so only the probe name tells their records apart.
const DBusInterface slot1Fru = {{"ADDRESS", uint64_t{80}},
                                {"BUS", uint64_t{16}}};
const std::string chassisId = scan::detail::getRecordName(
    slot1Fru, "Yosemite 4 Sentinel Dome Slot $bus % 15 Chassis");
const std::string cpuId = scan::detail::getRecordName(
    slot1Fru, "Yosemite 4 Sentinel Dome Slot $bus % 15 CPU");
const nlohmann::json chassisRecord = {
    {"Name", "Yosemite 4 Sentinel Dome Slot 1 Chassis"}, {"Type", "Chassis"}};
const nlohmann::json cpuRecord = {
    {"Name", "Yosemite 4 Sentinel Dome Slot 1 CPU"}, {"Type", "Cpu"}};

} // namespace

TEST(LogDevicInventory, RecordsToLogAddedEmptyBaselineReturnsAll)
{
    nlohmann::json newConfiguration = {{chassisId, chassisRecord},
                                       {cpuId, cpuRecord}};

    EXPECT_EQ(recordsToLogAdded(newConfiguration, {}), newConfiguration);
}

TEST(LogDevicInventory, RecordsToLogAddedSkipsBaselineKeys)
{
    nlohmann::json newConfiguration = {{chassisId, chassisRecord},
                                       {cpuId, cpuRecord}};

    EXPECT_EQ(recordsToLogAdded(newConfiguration, {chassisId}),
              (nlohmann::json{{cpuId, cpuRecord}}));
}

TEST(LogDevicInventory, RecordsToLogAddedReturnsRecordOnceRemovedFromBaseline)
{
    nlohmann::json newConfiguration = {{cpuId, cpuRecord}};
    std::unordered_set<std::string> cachedBaseline = {cpuId};

    EXPECT_TRUE(recordsToLogAdded(newConfiguration, cachedBaseline).empty());

    // An InventoryRemoved was reported for the CPU, so it is new again.
    cachedBaseline.erase(cpuId);

    EXPECT_EQ(recordsToLogAdded(newConfiguration, cachedBaseline),
              newConfiguration);
}

TEST(LogDevicInventory, RecordsToLogAddedIgnoresBaselineKeysNotPresent)
{
    nlohmann::json newConfiguration = {{cpuId, cpuRecord}};

    // The chassis is cached but not part of this scan.
    EXPECT_EQ(recordsToLogAdded(newConfiguration, {chassisId}),
              newConfiguration);
}

TEST(LogDevicInventory, RecordsToLogAddedNothingNew)
{
    EXPECT_TRUE(recordsToLogAdded(nlohmann::json::object(), {}).empty());
    EXPECT_TRUE(recordsToLogAdded(nlohmann::json(), {cpuId}).empty());
}
