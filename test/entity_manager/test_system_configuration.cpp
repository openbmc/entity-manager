#include "entity_manager/system_configuration.hpp"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

static nlohmann::json sampleBoard()
{
    return {{"Name", "Board"},
            {"Type", "Board"},
            {"Probe", "TRUE"},
            {"Exposes", nlohmann::json::array()}};
}

TEST(SystemConfiguration, ParsesBoardsByIdentifier)
{
    const nlohmann::json board = sampleBoard();
    const nlohmann::json input = {{"123", board}, {"456", board}};

    auto config = systemConfigurationFromJson(input);

    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->size(), 2);
    EXPECT_EQ(config->at("123").toJson(), board);
    EXPECT_EQ(config->at("456").toJson(), board);
}

TEST(SystemConfiguration, RejectsNonObjectRoot)
{
    EXPECT_FALSE(systemConfigurationFromJson(nullptr).has_value());
    EXPECT_FALSE(
        systemConfigurationFromJson(nlohmann::json::array()).has_value());
    EXPECT_FALSE(systemConfigurationFromJson("Board").has_value());
}

TEST(SystemConfiguration, AcceptsEmptyObject)
{
    auto config = systemConfigurationFromJson(nlohmann::json::object());

    ASSERT_TRUE(config.has_value());
    EXPECT_TRUE(config->empty());
}

TEST(SystemConfiguration, SkipsMalformedBoards)
{
    nlohmann::json missingName = sampleBoard();
    missingName.erase("Name");
    nlohmann::json invalidExpose = sampleBoard();
    invalidExpose["Exposes"].push_back(42);
    const nlohmann::json input = {{"valid", sampleBoard()},
                                  {"not-object", 42},
                                  {"missing-name", missingName},
                                  {"invalid-expose", invalidExpose}};

    auto config = systemConfigurationFromJson(input);

    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->size(), 1);
    EXPECT_EQ(config->at("valid").toJson(), sampleBoard());
}

TEST(SystemConfiguration, RemovesLegacyNullSlotsAndPreservesRuntimeProperties)
{
    nlohmann::json board = sampleBoard();
    const nlohmann::json sensor = {{"Name", "Runtime sensor"},
                                   {"Type", "TMP75"},
                                   {"Bus", 9},
                                   {"Address", 75}};
    board["Exposes"] = {nullptr, sensor, nullptr};
    board["xyz.openbmc_project.Inventory.Decorator.Asset"] = {
        {"Model", "runtime model"}};

    // Source-file parsing must still reject legacy null slots.
    EXPECT_FALSE(EMConfig::fromJson(board).has_value());
    const auto config = systemConfigurationFromJson({{"123", board}});

    ASSERT_TRUE(config.has_value());
    ASSERT_EQ(config->size(), 1U);
    board["Exposes"] = nlohmann::json::array({sensor});
    EXPECT_EQ(config->at("123").toJson(), board);
}
