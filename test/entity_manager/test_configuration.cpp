#include "entity_manager/configuration.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

// filterProbeInterfaces is protected, expose it to run it on configurations
// built in memory instead of loaded from disk.
class TestableConfiguration : public Configuration
{
  public:
    using Configuration::Configuration;
    using Configuration::filterProbeInterfaces;
};

static void addConfig(std::vector<EMConfig>& configurations,
                      const std::string& name,
                      const std::vector<std::string>& probe)
{
    nlohmann::json::object_t input;
    input["Exposes"] = nlohmann::json::array_t();
    input["Name"] = name;
    input["Probe"] = nlohmann::json::array_t();
    for (const std::string& statement : probe)
    {
        input["Probe"].emplace_back(statement);
    }
    input["Type"] = "Board";

    std::optional<EMConfig> optConfig = EMConfig::fromJson(input);
    ASSERT_TRUE(optConfig.has_value());
    configurations.push_back(optConfig.value());
}

// A configuration whose probe is empty or does not parse is dropped, the others
// stay in order and only they contribute probe interfaces.
TEST(Configuration, FilterProbeInterfacesDropsInvalidProbe)
{
    TestableConfiguration config({}, "");

    // Valid probe.
    addConfig(
        config.configurations, "Yosemite 4 Management Board",
        {"xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Management Board wBMC'})"});
    // The D-Bus property has no value, so the probe does not parse.
    addConfig(config.configurations,
              "Yosemite 4 Sentinel Dome T1 Slot $bus % 15",
              {"xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': })"});
    // Empty Probe array. It is not valid for the schema, but the schema is only
    // checked at runtime when runtime-validate-json is enabled.
    addConfig(config.configurations,
              "Yosemite 4 Sentinel Dome T2 Slot $bus % 15", {});
    // Valid probe.
    addConfig(
        config.configurations,
        "Yosemite 4 Sentinel Dome T1 with Retimer Slot $bus % 15",
        {"xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Sentinel Dome Retimer.*'})"});

    config.filterProbeInterfaces();

    ASSERT_EQ(config.configurations.size(), 2);
    EXPECT_EQ(config.configurations.at(0).name, "Yosemite 4 Management Board");
    EXPECT_EQ(config.configurations.at(1).name,
              "Yosemite 4 Sentinel Dome T1 with Retimer Slot $bus % 15");
    EXPECT_EQ(config.probeInterfaces, (std::unordered_set<std::string>{
                                          "xyz.openbmc_project.FruDevice"}));
}
