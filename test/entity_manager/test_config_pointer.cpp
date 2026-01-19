#include "entity_manager/config_pointer.hpp"
#include "entity_manager/em_config.hpp"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

static nlohmann::json::object_t getSampleConfigRecord()
{
    nlohmann::json::object_t threshold1;
    threshold1["Direction"] = "greater than";
    threshold1["Hysteresis"] = 0.8;
    threshold1["Name"] = "upper critical";
    threshold1["Severity"] = "1";
    threshold1["Value"] = "40";

    nlohmann::json::array_t thresholds;
    thresholds.emplace_back(threshold1);

    nlohmann::json::object_t configRecord1;
    configRecord1["Type"] = "TMP75";
    configRecord1["Name"] = "SCM_TEMP_C";
    configRecord1["Thresholds"] = thresholds;

    return configRecord1;
}

static EMConfig getSampleConfig()
{
    EMConfig input;
    input.name = "Santa Barbara SCM";
    input.type = "Board";

    return input;
}

TEST(ConfigPointer, writeBoard)
{
    SystemConfiguration systemConfiguration;
    systemConfiguration["823"] = EMConfig();

    ConfigPointer ptr("823");

    ptr.write(getSampleConfig().toJson(), systemConfiguration);

    ASSERT_TRUE(systemConfiguration.contains("823"));
}

TEST(ConfigPointer, writeExposesRecord)
{
    SystemConfiguration systemConfiguration;
    systemConfiguration["823"] = getSampleConfig();

    systemConfiguration["823"].exposesRecords.push_back(
        getSampleConfigRecord());

    ConfigPointer ptr("823", 0);

    nlohmann::json::object_t patch;
    patch["Name"] = "SCM_TEMP_2";
    patch["Type"] = "TMP";

    ptr.write(patch, systemConfiguration);

    ASSERT_TRUE(systemConfiguration.contains("823"));

    ASSERT_EQ(systemConfiguration["823"].exposesRecords.size(), 1);

    ASSERT_TRUE(systemConfiguration["823"].exposesRecords[0].contains("Type"));
    EXPECT_EQ(systemConfiguration["823"].exposesRecords[0]["Type"], "TMP");

    ASSERT_TRUE(systemConfiguration["823"].exposesRecords[0].contains("Name"));
    EXPECT_EQ(systemConfiguration["823"].exposesRecords[0]["Name"],
              "SCM_TEMP_2");
}

TEST(ConfigPointer, writeConfigProperty)
{
    SystemConfiguration systemConfiguration;
    systemConfiguration["823"] = getSampleConfig();

    systemConfiguration["823"].exposesRecords.push_back(
        getSampleConfigRecord());

    ConfigPointer ptr("823", 0, "Name");

    nlohmann::json patch = "OTHER_NAME";

    ptr.write(patch, systemConfiguration);

    ASSERT_TRUE(systemConfiguration.contains("823"));

    ASSERT_EQ(systemConfiguration["823"].exposesRecords.size(), 1);

    ASSERT_TRUE(systemConfiguration["823"].exposesRecords[0].contains("Name"));
    EXPECT_EQ(systemConfiguration["823"].exposesRecords[0]["Name"],
              "OTHER_NAME");
}

TEST(ConfigPointer, writeConfigArrayProperty)
{
    SystemConfiguration systemConfiguration;
    systemConfiguration["823"] = getSampleConfig();

    systemConfiguration["823"].exposesRecords.push_back(
        getSampleConfigRecord());

    ConfigPointer ptr("823", 0, "Thresholds", 0);

    nlohmann::json threshold1;
    threshold1["Direction"] = "greater than";
    threshold1["Hysteresis"] = 0.8;
    threshold1["Name"] = "lower critical";
    threshold1["Severity"] = "1";
    threshold1["Value"] = "10";

    ptr.write(threshold1, systemConfiguration);

    ASSERT_TRUE(systemConfiguration.contains("823"));

    ASSERT_EQ(systemConfiguration["823"].exposesRecords.size(), 1);

    ASSERT_TRUE(
        systemConfiguration["823"].exposesRecords[0].contains("Thresholds"));
    ASSERT_TRUE(
        systemConfiguration["823"].exposesRecords[0]["Thresholds"].is_array());
    ASSERT_EQ(systemConfiguration["823"].exposesRecords[0]["Thresholds"].size(),
              1);
    EXPECT_EQ(
        systemConfiguration["823"].exposesRecords[0]["Thresholds"][0]["Value"],
        "10");
}

TEST(ConfigPointer, writeNestedInterfaceProperties)
{
    SystemConfiguration systemConfiguration;
    systemConfiguration["823"] = getSampleConfig();
    systemConfiguration["823"]
        .extraInterfaces["xyz.openbmc_project.Inventory.Decorator.Asset"] = {
        {"Model", "old"}};
    systemConfiguration["823"].exposesRecords.push_back(
        getSampleConfigRecord());
    systemConfiguration["823"].exposesRecords[0]["Polling"] = {{"Interval", 1}};

    EXPECT_TRUE(
        ConfigPointer("823", "xyz.openbmc_project.Inventory.Decorator.Asset")
            .withName("Model")
            .write("new", systemConfiguration));
    EXPECT_TRUE(ConfigPointer("823", 0, "Polling")
                    .withName("Interval")
                    .write(2, systemConfiguration));
    EXPECT_TRUE(ConfigPointer("823", 0, "Thresholds", 0)
                    .withName("Value")
                    .write("15", systemConfiguration));

    EXPECT_EQ(systemConfiguration["823"].extraInterfaces
                  ["xyz.openbmc_project.Inventory.Decorator.Asset"]["Model"],
              "new");
    EXPECT_EQ(
        systemConfiguration["823"].exposesRecords[0]["Polling"]["Interval"], 2);
    EXPECT_EQ(
        systemConfiguration["823"].exposesRecords[0]["Thresholds"][0]["Value"],
        "15");
    EXPECT_EQ(
        systemConfiguration["823"].exposesRecords[0]["Thresholds"][0]["Name"],
        "upper critical");
}

TEST(ConfigPointer, deleteKeepsExposeIndices)
{
    SystemConfiguration systemConfiguration;
    systemConfiguration["823"] = getSampleConfig();
    systemConfiguration["823"].exposesRecords.push_back(
        getSampleConfigRecord());
    systemConfiguration["823"].exposesRecords.push_back(
        getSampleConfigRecord());

    EXPECT_TRUE(ConfigPointer("823", 0).write(nullptr, systemConfiguration));
    EXPECT_TRUE(ConfigPointer("823", 1).withName("Name").write(
        "second", systemConfiguration));

    EXPECT_EQ(systemConfiguration["823"].exposesRecords[0]["Status"],
              "disabled");
    EXPECT_EQ(systemConfiguration["823"].exposesRecords[1]["Name"], "second");

    EXPECT_TRUE(ConfigPointer("823", 1, "Thresholds", 0)
                    .write(nullptr, systemConfiguration));
    EXPECT_TRUE(systemConfiguration["823"]
                    .exposesRecords[1]["Thresholds"][0]
                    .is_null());
    EXPECT_EQ(systemConfiguration["823"].exposesRecords[1]["Name"], "second");
}

TEST(ConfigPointer, reusesDeletedExposeSlot)
{
    SystemConfiguration systemConfiguration;
    EMConfig& board = systemConfiguration["823"];
    board = getSampleConfig();
    board.exposesRecords.push_back(getSampleConfigRecord());
    board.exposesRecords.push_back(getSampleConfigRecord());
    board.exposesRecords[1]["Name"] = "second";
    nlohmann::json expected = board.toJson();

    ASSERT_TRUE(ConfigPointer("823", 0).write(nullptr, systemConfiguration));

    expected["Exposes"][0]["Status"] = "disabled";
    ASSERT_EQ(board.exposesRecords.size(), 2);
    EXPECT_EQ(board.toJson(), expected);

    const nlohmann::json::object_t replacement = {{"Name", "replacement"},
                                                  {"Type", "TMP75"}};
    ASSERT_TRUE(
        ConfigPointer("823", 0).write(replacement, systemConfiguration));

    expected["Exposes"][0] = replacement;
    ASSERT_EQ(board.exposesRecords.size(), 2);
    EXPECT_EQ(board.toJson(), expected);
}

class ConfigPointerFailureTest : public ::testing::Test
{
  protected:
    SystemConfiguration systemConfiguration;

    void SetUp() override
    {
        systemConfiguration["823"] = getSampleConfig();
        systemConfiguration["823"].exposesRecords.push_back(
            getSampleConfigRecord());
        systemConfiguration["823"].exposesRecords[0]["Polling"] = {
            {"Interval", 1}};
    }

    nlohmann::json snapshotConfiguration() const
    {
        nlohmann::json result = nlohmann::json::object();
        for (const auto& [id, config] : systemConfiguration)
        {
            result[id] = config.toJson();
        }
        return result;
    }

    void expectWriteRejected(const ConfigPointer& ptr)
    {
        const nlohmann::json before = snapshotConfiguration();

        EXPECT_FALSE(ptr.write("new", systemConfiguration));
        EXPECT_EQ(snapshotConfiguration(), before);
    }
};

TEST_F(ConfigPointerFailureTest, rejectsMissingBoard)
{
    expectWriteRejected(ConfigPointer("missing"));
}

TEST_F(ConfigPointerFailureTest, rejectsMissingProperty)
{
    expectWriteRejected(ConfigPointer("823", "missing"));
    expectWriteRejected(ConfigPointer("823", 0, "missing"));
}

TEST_F(ConfigPointerFailureTest, rejectsMissingMember)
{
    expectWriteRejected(ConfigPointer("823", 0, "Polling").withName("missing"));
    expectWriteRejected(
        ConfigPointer("823", 0, "Thresholds", 0).withName("missing"));
}

TEST_F(ConfigPointerFailureTest, rejectsOutOfRangeExposeIndex)
{
    expectWriteRejected(ConfigPointer("823", 1));
}

TEST_F(ConfigPointerFailureTest, rejectsOutOfRangeArrayIndex)
{
    expectWriteRejected(ConfigPointer("823", 0, "Thresholds", 1));
}

TEST_F(ConfigPointerFailureTest, rejectsTraversalThroughDeletedProperty)
{
    ASSERT_TRUE(
        ConfigPointer("823", 0, "Polling").write(nullptr, systemConfiguration));

    expectWriteRejected(
        ConfigPointer("823", 0, "Polling").withName("Interval"));
}

TEST_F(ConfigPointerFailureTest, rejectsTraversalThroughDeletedArrayElement)
{
    ASSERT_TRUE(ConfigPointer("823", 0, "Thresholds", 0)
                    .write(nullptr, systemConfiguration));

    expectWriteRejected(
        ConfigPointer("823", 0, "Thresholds", 0).withName("Value"));
}
