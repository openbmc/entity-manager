#include "entity_manager/probe_lexer.hpp"
#include "entity_manager/probe_parser.hpp"

#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using probe::Ast;
using probe::DbusProbe;
using probe::lexProbe;
using probe::Node;
using probe::ParsedProbe;
using probe::parseProbe;
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

static std::optional<Ast> parse(std::string_view probe)
{
    std::optional<std::vector<Token>> tokens = lexProbe(probe);

    if (!tokens.has_value())
    {
        throw std::invalid_argument("did not lex");
    }

    return parseProbe(tokens.value());
}

// Unwraps the way test_em_config does, since the unchecked optional access
// check of clang-tidy also reports an access that only follows an assertion.
static Ast parseValid(std::string_view probe)
{
    std::optional<Ast> optAst = parse(probe);

    if (!optAst.has_value())
    {
        throw std::invalid_argument("did not parse");
    }

    return std::move(optAst.value());
}

// Writes the tree as nested operators, for example AND(OR(TRUE,FALSE),TRUE),
// so a test can state the structure it expects in one string.
static std::string render(const Node& node)
{
    switch (node.token.type)
    {
        case TokenType::opAnd:
            return "AND(" + render(*node.left) + "," + render(*node.right) +
                   ")";
        case TokenType::opOr:
            return "OR(" + render(*node.left) + "," + render(*node.right) + ")";
        case TokenType::opNot:
            return "NOT(" + render(*node.left) + ")";
        case TokenType::boolTrue:
            return "TRUE";
        case TokenType::boolFalse:
            return "FALSE";
        case TokenType::found:
            return "FOUND(" + node.token.value + ")";
        case TokenType::dbusProbe:
            if (!node.token.dbus.has_value())
            {
                throw std::invalid_argument("a D-Bus probe without its part");
            }
            return "DBUS(" + node.token.dbus.value().interface + ")";
        case TokenType::matchOne:
            break;
    }
    return "";
}

// A Probe with no operator is a tree of one operand.
TEST(ParseProbe, SingleOperand)
{
    const Ast ast = parseValid("TRUE");

    EXPECT_EQ(render(*ast.root), "TRUE");
    EXPECT_FALSE(ast.matchOne);
}

// AND and OR have the same precedence and the left side of an operator is
// everything before it, which is how Probes that mix them are read today.
TEST(ParseProbe, OperatorsAssociateToTheLeft)
{
    const Ast mixed =
        parseValid("TRUE OR FALSE AND FOUND('Yosemite 4 Management Board')");

    EXPECT_EQ(render(*mixed.root),
              "AND(OR(TRUE,FALSE),FOUND(Yosemite 4 Management Board))");
}

// A D-Bus probe is an operand like TRUE and FOUND.
TEST(ParseProbe, DbusProbeIsAnOperand)
{
    const Ast ast = parseValid(mgmtBoardWBmc + " OR " + mgmtBoardWnBmc);

    EXPECT_EQ(render(*ast.root), "OR(DBUS(xyz.openbmc_project.FruDevice),"
                                 "DBUS(xyz.openbmc_project.FruDevice))");
}

// MATCH_ONE applies to the whole result, so it is a flag and not a node.
TEST(ParseProbe, TrailingMatchOneIsAFlag)
{
    const Ast ast = parseValid("TRUE AND FALSE MATCH_ONE");

    EXPECT_EQ(render(*ast.root), "AND(TRUE,FALSE)");
    EXPECT_TRUE(ast.matchOne);
}

// NOT applies to the operand right after it, not to the rest of the Probe.
TEST(ParseProbe, NotBindsToTheNextOperand)
{
    EXPECT_EQ(render(*parseValid("NOT TRUE").root), "NOT(TRUE)");
    EXPECT_EQ(render(*parseValid("NOT TRUE AND FALSE").root),
              "AND(NOT(TRUE),FALSE)");
    EXPECT_EQ(render(*parseValid("TRUE OR NOT FALSE").root),
              "OR(TRUE,NOT(FALSE))");
    EXPECT_EQ(render(*parseValid("NOT " + mgmtBoardWBmc).root),
              "NOT(DBUS(xyz.openbmc_project.FruDevice))");
}

// A pair of NOTs cancels out, so an operand has at most one NOT above it.
TEST(ParseProbe, PairsOfNotsCancelOut)
{
    EXPECT_EQ(render(*parseValid("NOT NOT TRUE").root), "TRUE");
    EXPECT_EQ(render(*parseValid("NOT NOT NOT TRUE").root), "NOT(TRUE)");
    EXPECT_EQ(render(*parseValid("FALSE OR NOT NOT TRUE").root),
              "OR(FALSE,TRUE)");
}

// NOT needs TRUE, FALSE or a D-Bus probe after it. FOUND depends on the order
// in which configurations are probed, so it cannot be negated.
TEST(ParseProbe, RejectsMisplacedNot)
{
    EXPECT_FALSE(parse("NOT FOUND('A')").has_value());
    EXPECT_FALSE(parse("NOT NOT FOUND('A')").has_value());
    EXPECT_FALSE(parse("TRUE AND NOT FOUND('A')").has_value());
    EXPECT_FALSE(parse("TRUE NOT AND FALSE").has_value());
    EXPECT_FALSE(parse("FALSE NOT OR TRUE").has_value());
    EXPECT_FALSE(parse("NOT MATCH_ONE TRUE").has_value());
    EXPECT_FALSE(parse("TRUE NOT MATCH_ONE").has_value());
    EXPECT_FALSE(parse("TRUE AND NOT").has_value());
    EXPECT_FALSE(parse("NOT").has_value());
}

// AND and OR need an operand on both sides.
TEST(ParseProbe, RejectsAnOperatorWithoutAnOperand)
{
    EXPECT_FALSE(parse("AND TRUE").has_value());
    EXPECT_FALSE(parse("TRUE OR").has_value());
    EXPECT_FALSE(parse("TRUE AND AND").has_value());
    EXPECT_FALSE(parse("TRUE AND OR").has_value());
}

// Two operands in a row need an AND or an OR between them.
TEST(ParseProbe, RejectsOperandsWithoutAnOperator)
{
    EXPECT_FALSE(parse("TRUE FALSE").has_value());
}

// MATCH_ONE may only be the last token, and only once.
TEST(ParseProbe, RejectsMatchOneAnywhereElse)
{
    EXPECT_FALSE(parse("TRUE AND MATCH_ONE").has_value());
    EXPECT_FALSE(parse("TRUE MATCH_ONE AND FALSE").has_value());
    EXPECT_FALSE(parse("TRUE MATCH_ONE MATCH_ONE").has_value());
}

// The statements of the array form are joined and lexed as one Probe, so the
// operator between two boards can be an element of its own.
TEST(ParseProbeCommand, ArrayOfStatementsJoinedByOperator)
{
    std::optional<ParsedProbe> parsed =
        parseProbeCommand({mgmtBoardWBmc, "OR", mgmtBoardWnBmc});

    if (!parsed.has_value())
    {
        throw std::invalid_argument("did not parse");
    }
    EXPECT_EQ(parsed->tokens,
              (std::vector<Token>{fruDevice("Management Board wBMC"),
                                  {TokenType::opOr, ""},
                                  fruDevice("Management Board wN-BMC")}));
}

// A statement that does not lex rejects the whole Probe, wherever it is, so
// the valid statements around it are not used on their own.
TEST(ParseProbeCommand, BrokenStatementRejectsTheProbe)
{
    EXPECT_FALSE(
        parseProbeCommand({brokenStatement, "OR", mgmtBoardWBmc}).has_value());
    EXPECT_FALSE(
        parseProbeCommand({mgmtBoardWBmc, "OR", brokenStatement}).has_value());
    EXPECT_FALSE(parseProbeCommand({mgmtBoardWBmc + " OR " + brokenStatement})
                     .has_value());
}

// A Probe with no statement, or an empty one, is rejected.
TEST(ParseProbeCommand, EmptyProbeIsRejected)
{
    EXPECT_FALSE(parseProbeCommand({}).has_value());
    EXPECT_FALSE(parseProbeCommand({""}).has_value());
}

// Two statements that lex fine are not a Probe without an operator element
// between them.
TEST(ParseProbeCommand, MissingOperatorBetweenStatementsRejectsTheProbe)
{
    EXPECT_FALSE(
        parseProbeCommand({mgmtBoardWBmc, mgmtBoardWnBmc}).has_value());
}
