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

    std::cout << nlohmann::json(systemConfiguration["823"].toJson()).dump(4)
              << std::endl
              << std::endl;

    ptr.write(threshold1, systemConfiguration);

    ASSERT_TRUE(systemConfiguration.contains("823"));

    ASSERT_EQ(systemConfiguration["823"].exposesRecords.size(), 1);

    std::cout << nlohmann::json(systemConfiguration["823"].toJson()).dump(4)
              << std::endl
              << std::endl;

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
