#include "entity_manager/probe_lexer.hpp"

#include <string>
#include <vector>

#include <gtest/gtest.h>

using probe::DbusProbe;
using probe::lexProbe;
using probe::parseProbeCommand;
using probe::Token;
using probe::TokenType;

// Bare keywords lex to the matching token type with an empty value.
TEST(LexProbe, Keywords)
{
    EXPECT_EQ(lexProbe("TRUE"),
              (std::vector<Token>{{TokenType::boolTrue, ""}}));
    EXPECT_EQ(lexProbe("FALSE"),
              (std::vector<Token>{{TokenType::boolFalse, ""}}));
    EXPECT_EQ(lexProbe("AND"), (std::vector<Token>{{TokenType::opAnd, ""}}));
    EXPECT_EQ(lexProbe("OR"), (std::vector<Token>{{TokenType::opOr, ""}}));
    EXPECT_EQ(lexProbe("MATCH_ONE"),
              (std::vector<Token>{{TokenType::matchOne, ""}}));
}

// FOUND('name') yields a found token whose value is the unquoted name.
TEST(LexProbe, FoundFunction)
{
    EXPECT_EQ(lexProbe("FOUND('Mt.Jade')"),
              (std::vector<Token>{{TokenType::found, "Mt.Jade"}}));
    EXPECT_EQ(lexProbe("FOUND('Harma SCM')"),
              (std::vector<Token>{{TokenType::found, "Harma SCM"}}));
}

// A D-Bus probe contains the parsed interface/properties with an empty value.
TEST(LexProbe, DbusProbe)
{
    const std::string dbus =
        "xyz.openbmc_project.FruDevice({'BUS': 6, 'ADDRESS': 80})";
    EXPECT_EQ(lexProbe(dbus),
              (std::vector<Token>{{TokenType::dbusProbe, "",
                                   DbusProbe{"xyz.openbmc_project.FruDevice",
                                             {{"BUS", 6}, {"ADDRESS", 80}}}}}));
}

// The regression this change fixes: a keyword appearing as a substring inside a
// D-Bus probe must not be tokenized as an operator.
TEST(LexProbe, KeywordSubstringInsideDbusProbeIsNotAnOperator)
{
    const std::string dbus =
        "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'HORIZON'})";
    EXPECT_EQ(
        lexProbe(dbus),
        (std::vector<Token>{{TokenType::dbusProbe, "",
                             DbusProbe{"xyz.openbmc_project.FruDevice",
                                       {{"BOARD_PRODUCT_NAME", "HORIZON"}}}}}));
}

// The mixed operator pattern used by several platforms:
// A OR B AND FOUND('X').
TEST(LexProbe, MixedOperators)
{
    const std::string a = "xyz.openbmc_project.FruDevice({'BUS': 6})";
    const std::string b = "xyz.openbmc_project.FruDevice({'BUS': 7})";
    EXPECT_EQ(lexProbe(a + " OR " + b + " AND FOUND('Board')"),
              (std::vector<Token>{
                  {TokenType::dbusProbe, "",
                   DbusProbe{"xyz.openbmc_project.FruDevice", {{"BUS", 6}}}},
                  {TokenType::opOr, ""},
                  {TokenType::dbusProbe, "",
                   DbusProbe{"xyz.openbmc_project.FruDevice", {{"BUS", 7}}}},
                  {TokenType::opAnd, ""},
                  {TokenType::found, "Board"}}));
}

// Surrounding and separating whitespace is tolerated.
TEST(LexProbe, WhitespaceTolerated)
{
    EXPECT_EQ(lexProbe("  TRUE   AND\tFALSE "),
              (std::vector<Token>{{TokenType::boolTrue, ""},
                                  {TokenType::opAnd, ""},
                                  {TokenType::boolFalse, ""}}));
}

// A keyword appearing inside a quoted value (with regex metacharacters and an
// escaped quote) must not break the single D-Bus probe token.
TEST(LexProbe, QuotedValueWithParensAndEscapedQuote)
{
    const std::string dbus = "xyz.a.C({'N': 'P(31|33) is it\\'s AND (x)'})";
    // Preserve the existing property parser's rejection of this escaping.
    EXPECT_EQ(lexProbe(dbus), std::nullopt);
}

TEST(LexProbe, BalancedNestedDelimiters)
{
    const std::string dbus = "xyz.Iface({'A': (1), 'B': {'C': 2}})";
    // Balanced delimiters do not make parenthesized values valid JSON.
    EXPECT_EQ(lexProbe(dbus), std::nullopt);
}

TEST(LexProbe, ParsesDbusPropertiesBeforeEvaluation)
{
    const std::string dbus =
        R"(xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Riser\d+', 'ADDRESS': 80}))";
    EXPECT_EQ(lexProbe(dbus),
              (std::vector<Token>{
                  {TokenType::dbusProbe, "",
                   DbusProbe{"xyz.openbmc_project.FruDevice",
                             {{"BOARD_PRODUCT_NAME", "Riser\\d+"},
                              {"ADDRESS", 80}}}}}));
}

TEST(LexProbe, EmptyDbusProperties)
{
    EXPECT_EQ(
        lexProbe("xyz.openbmc_project.FruDevice({})"),
        (std::vector<Token>{{TokenType::dbusProbe, "",
                             DbusProbe{"xyz.openbmc_project.FruDevice", {}}}}));
}

TEST(LexProbe, PreservesMultipleBackslashesInDbusProperties)
{
    const std::string dbus =
        R"(xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Riser\d+\\slot\w+'}))";
    EXPECT_EQ(
        lexProbe(dbus),
        (std::vector<Token>{
            {TokenType::dbusProbe, "",
             DbusProbe{"xyz.openbmc_project.FruDevice",
                       {{"BOARD_PRODUCT_NAME", R"(Riser\d+\\slot\w+)"}}}}}));
}

TEST(LexProbe, RejectsMalformedDbusProperties)
{
    EXPECT_EQ(lexProbe("xyz.Iface({'A': })"), std::nullopt);
    EXPECT_EQ(lexProbe("xyz.Iface([1, 2])"), std::nullopt);
    EXPECT_EQ(lexProbe("xyz.Iface(null)"), std::nullopt);
    EXPECT_EQ(lexProbe("xyz.Iface({'A': 1}) OR xyz.Iface({'B': })"),
              std::nullopt);
}

// Newlines and carriage returns act as token separators.
TEST(LexProbe, NewlineSeparatesTokens)
{
    EXPECT_EQ(lexProbe("TRUE\nAND\r\nFALSE"),
              (std::vector<Token>{{TokenType::boolTrue, ""},
                                  {TokenType::opAnd, ""},
                                  {TokenType::boolFalse, ""}}));
}

// An empty or whitespace-only probe yields no tokens (not an error).
TEST(LexProbe, EmptyIsNoTokens)
{
    EXPECT_EQ(lexProbe(""), (std::vector<Token>{}));
    EXPECT_EQ(lexProbe("   "), (std::vector<Token>{}));
}

// Malformed input is a syntax error.
TEST(LexProbe, SyntaxErrors)
{
    // A bare word that is not a keyword.
    EXPECT_EQ(lexProbe("NOTAKEYWORD"), std::nullopt);
    // Unbalanced parenthesis.
    EXPECT_EQ(lexProbe("xyz.Iface({'A': 1}"), std::nullopt);
    // A closing brace cannot close a parenthesis (or vice versa).
    EXPECT_EQ(lexProbe("xyz.Iface({'A': (1})"), std::nullopt);
    EXPECT_EQ(lexProbe("xyz.Iface({'A': {1)})"), std::nullopt);
    EXPECT_EQ(lexProbe("FOUND('A'}"), std::nullopt);
    // A stray delimiter that does not start a token.
    EXPECT_EQ(lexProbe("{'A': 1}"), std::nullopt);
    // Unterminated quote (keeps the paren depth open to end of input).
    EXPECT_EQ(lexProbe("FOUND('unterminated"), std::nullopt);
    EXPECT_EQ(lexProbe("xyz.Iface({'A': 'unterminated})"), std::nullopt);
}

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
