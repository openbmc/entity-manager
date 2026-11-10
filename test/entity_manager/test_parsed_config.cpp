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

    if (!parsed.has_value())
    {
        FAIL() << "did not parse";
    }
    const std::vector<probe::Token>& tokens = parsed.value().tokens();
    ASSERT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens.at(0).type, probe::TokenType::dbusProbe);
    EXPECT_EQ(tokens.at(1).type, probe::TokenType::opOr);
    const probe::Token& last = tokens.at(2);
    if (!last.dbus.has_value())
    {
        FAIL() << "no dbus probe";
    }
    EXPECT_EQ(last.dbus->properties.at("BOARD_PRODUCT_NAME"),
              "Management Board wN-BMC");
}

// The Probe of a ParsedConfig is never empty.
TEST(ParsedConfig, MakeRejectsEmptyProbe)
{
    EMConfig config;
    config.name = "Yosemite 4 Management Board";
    config.type = "Board";

    EXPECT_FALSE(ParsedConfig::make(std::move(config)).has_value());
}

// A Probe that does not parse is not a ParsedConfig either.
TEST(ParsedConfig, MakeRejectsProbeThatDoesNotParse)
{
    EMConfig config;
    config.name = "Yosemite 4 Management Board";
    config.type = "Board";
    config.probeStmt = {"xyz.openbmc_project.FruDevice({'A': })"};

    EXPECT_FALSE(ParsedConfig::make(std::move(config)).has_value());
}
