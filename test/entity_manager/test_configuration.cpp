#include "entity_manager/configuration.hpp"

#include <unistd.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#ifndef SCHEMA_DIR
#define SCHEMA_DIR "."
#endif

class ConfigurationTest : public ::testing::Test
{
  protected:
    std::filesystem::path tmpDir;

    void SetUp() override
    {
        tmpDir = std::filesystem::temp_directory_path() /
                 ("em_configuration_test_" + std::to_string(getpid()));
        std::filesystem::create_directories(tmpDir);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(tmpDir);
    }

    void writeFile(const std::string& name, const nlohmann::json& content)
    {
        std::ofstream file{tmpDir / name};
        file << content.dump();
    }
};

static nlohmann::json makeRecord(const std::string& name,
                                 const std::vector<std::string>& probe)
{
    return {{"Exposes", nlohmann::json::array()},
            {"Name", name},
            {"Probe", probe},
            {"Type", "Board"}};
}

// A configuration whose probe is empty or does not parse is not added, the
// others stay in order and only they contribute probe interfaces.
TEST_F(ConfigurationTest, DropsConfigurationsWithInvalidProbe)
{
    // A file with an array of records.
    writeFile(
        "a.json",
        nlohmann::json::array(
            {makeRecord(
                 "Yosemite 4 Management Board",
                 {"xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Management Board wBMC'})"}),
             // The D-Bus property has no value, so the probe does not parse.
             makeRecord(
                 "Yosemite 4 Sentinel Dome T1 Slot $bus % 15",
                 {"xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': })"}),
             makeRecord(
                 "Yosemite 4 Sentinel Dome T1 with Retimer Slot $bus % 15",
                 {"xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Sentinel Dome Retimer.*'})"})}));
    // A file with a single record.
    writeFile("b.json",
              makeRecord(
                  "Yosemite 4 Sentinel Dome T2 Slot $bus % 15",
                  {"xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': })"}));
    // Empty Probe array. It is not valid for the schema, but the schema is only
    // checked at runtime when runtime-validate-json is enabled.
    writeFile(
        "c.json",
        makeRecord("Yosemite 4 Sentinel Dome T2 with Retimer Slot $bus % 15",
                   {}));

    Configuration config({tmpDir}, std::filesystem::path{SCHEMA_DIR});

    ASSERT_EQ(config.configurations.size(), 2);
    EXPECT_EQ(config.configurations.at(0).name, "Yosemite 4 Management Board");
    EXPECT_EQ(config.configurations.at(1).name,
              "Yosemite 4 Sentinel Dome T1 with Retimer Slot $bus % 15");
    EXPECT_EQ(config.probeInterfaces, (std::unordered_set<std::string>{
                                          "xyz.openbmc_project.FruDevice"}));
}
