#include "entity_manager/em_config.hpp"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

static nlohmann::json::object_t getSampleConfig()
{
    nlohmann::json::object_t threshold1;
    threshold1["Direction"] = "greater than";
    threshold1["Hystersis"] = 0.8;
    threshold1["Name"] = "upper critical";
    threshold1["Severity"] = "1";
    threshold1["Value"] = "40";

    nlohmann::json::object_t configRecord1;
    configRecord1["Type"] = "TMP75";
    configRecord1["Name"] = "SCM_TEMP_C";
    configRecord1["Bus"] = "9";
    configRecord1["Address"] = "0x4b";
    configRecord1["Thresholds"] = nlohmann::json::array_t();
    configRecord1["Thresholds"].emplace_back(threshold1);

    nlohmann::json::array_t exposesArray;
    exposesArray.emplace_back(configRecord1);

    nlohmann::json::object_t input;
    input["Exposes"] = exposesArray;
    input["Name"] = "Santa Barbara SCM";
    input["Probe"] = nlohmann::json::array_t();
    input["Probe"].emplace_back(
        "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Santa Barbara SCM'})");
    input["Type"] = "Board";
    input["Logging"] = "Off";
    input["xyz.openbmc_project.Inventory.Item.System"] =
        nlohmann::json::object_t();

    return input;
}

TEST(EMConfig, ParseNameAndType)
{
    const nlohmann::json::object_t input = getSampleConfig();

    EMConfig config(input);

    EXPECT_FALSE(config.deviceHasLogging);
    EXPECT_EQ(config.name, "Santa Barbara SCM");
    EXPECT_EQ(config.type, "Board");
}

TEST(EMConfig, ParseProbeSingle)
{
    const nlohmann::json::object_t input = getSampleConfig();

    EMConfig config(input);

    EXPECT_EQ(config.probeStmt.size(), 1);
    EXPECT_TRUE(
        config.probeStmt[0].starts_with("xyz.openbmc_project.FruDevice"));
}

TEST(EMConfig, ParseProbeArray)
{
    nlohmann::json::object_t input = getSampleConfig();

    input["Probe"] = nlohmann::json::array_t();
    input["Probe"].emplace_back(
        "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': '.*SBP1'})");
    input["Probe"].emplace_back("AND");
    input["Probe"].emplace_back(
        "xyz.openbmc_project.Inventory.Item({'PrettyName': 'RSSD03', 'Present': true})");
    input["Probe"].emplace_back("MATCH_ONE");

    EMConfig config(input);

    EXPECT_EQ(config.probeStmt.size(), 4);
    EXPECT_EQ(config.probeStmt[1], "AND");
}

TEST(EMConfig, ParseExposes)
{
    const nlohmann::json::object_t input = getSampleConfig();

    EMConfig config(input);

    EXPECT_EQ(config.exposesRecords.size(), 1);
    EXPECT_EQ(config.exposesRecords[0]["Name"], "SCM_TEMP_C");
    EXPECT_EQ(config.exposesRecords[0]["Type"], "TMP75");
}

TEST(EMConfig, ParseExtraInterfaces)
{
    const nlohmann::json::object_t input = getSampleConfig();

    EMConfig config(input);

    EXPECT_EQ(config.extraInterfaces.size(), 1);
    EXPECT_TRUE(config.extraInterfaces.contains(
        "xyz.openbmc_project.Inventory.Item.System"));
    EXPECT_EQ(config
                  .extraInterfaces["xyz.openbmc_project.Inventory.Item.System"]
                  .size(),
              0);
}

TEST(EMConfig, RoundTrip)
{
    // Test that EMConfig can reproduce the config which was passed into it.
    const nlohmann::json::object_t input = getSampleConfig();

    EMConfig config(input);

    const nlohmann::json out = config.toJson();

    const nlohmann::json::object_t* outObj =
        out.get_ptr<const nlohmann::json::object_t*>();

    ASSERT_TRUE(outObj != nullptr);

    EXPECT_EQ(*outObj, input);
}
