#include "entity_manager/probe_lexer.hpp"
#include "entity_manager/probe_parser.hpp"

#include <string>
#include <vector>

#include <gtest/gtest.h>

using probe::DbusProbe;
using probe::parseProbeCommand;
using probe::Token;
using probe::TokenType;

// Probe statements below are the ones the Yosemite 4 configurations use.
static const std::string mgmtBoardWBmc =
    "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Management Board "
    "wBMC', 'PRODUCT_PRODUCT_NAME': 'Yosemite V4'})";
static const std::string mgmtBoardWnBmc =
    "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Management Board "
    "wN-BMC', 'PRODUCT_PRODUCT_NAME': 'Yosemite V4'})";
// A FruDevice statement whose last property has no value.
static const std::string brokenStatement =
    "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Sentinel Dome', "
    "'PRODUCT_PRODUCT_NAME': })";

static Token fruDevice(const std::string& board)
{
    return {TokenType::dbusProbe, "",
            DbusProbe{"xyz.openbmc_project.FruDevice",
                      {{"BOARD_PRODUCT_NAME", board},
                       {"PRODUCT_PRODUCT_NAME", "Yosemite V4"}}}};
}

// The statements of the array form are joined and lexed as one Probe, so the
// operator between two boards can be an element of its own.
TEST(ParseProbeCommand, ArrayOfStatementsJoinedByOperator)
{
    EXPECT_EQ(parseProbeCommand({mgmtBoardWBmc, "OR", mgmtBoardWnBmc}),
              (std::vector<Token>{fruDevice("Management Board wBMC"),
                                  {TokenType::opOr, ""},
                                  fruDevice("Management Board wN-BMC")}));
}

// A statement that does not lex makes the whole Probe empty, wherever it is,
// so the valid statements around it are not used on their own.
TEST(ParseProbeCommand, BrokenStatementEmptiesTheProbe)
{
    EXPECT_TRUE(
        parseProbeCommand({brokenStatement, "OR", mgmtBoardWBmc}).empty());
    EXPECT_TRUE(
        parseProbeCommand({mgmtBoardWBmc, "OR", brokenStatement}).empty());
    EXPECT_TRUE(
        parseProbeCommand({mgmtBoardWBmc + " OR " + brokenStatement}).empty());
}

// A Probe with no statement, or an empty one, has no tokens.
TEST(ParseProbeCommand, EmptyProbeHasNoTokens)
{
    EXPECT_TRUE(parseProbeCommand({}).empty());
    EXPECT_TRUE(parseProbeCommand({""}).empty());
}
