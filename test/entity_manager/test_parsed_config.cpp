#include "entity_manager/parsed_config.hpp"

#include <optional>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

// The tokens cover every statement of the Probe, not only the first.
TEST(ParsedConfig, TokensCoverAllProbeStatements)
{
    EMConfig config;
    config.name = "Yosemite 4 Management Board";
    config.type = "Board";
    config.probeStmt = {
        "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Management Board wBMC'})",
        "OR",
        "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Management Board wN-BMC'})"};

    std::optional<ParsedConfig> parsed = ParsedConfig::make(std::move(config));

    ASSERT_TRUE(parsed.has_value());
    const std::vector<probe::Token>& tokens = parsed.value().tokens();
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens.at(0).type, probe::TokenType::dbusProbe);
    EXPECT_EQ(tokens.at(1).type, probe::TokenType::opOr);
    const probe::Token& last = tokens.at(2);
    ASSERT_TRUE(last.dbus.has_value());
    EXPECT_EQ(last.dbus->properties.at("BOARD_PRODUCT_NAME"),
              "Management Board wN-BMC");
}
