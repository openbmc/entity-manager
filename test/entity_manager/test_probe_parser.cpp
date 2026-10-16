#include "entity_manager/probe_lexer.hpp"
#include "entity_manager/probe_parser.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

using probe::AstNode;
using probe::lexProbe;
using probe::parseProbe;
using probe::ProbeAst;
using probe::Token;
using probe::TokenType;

namespace
{

AstNode leaf(TokenType type, std::string value = "")
{
    AstNode node;
    node.leafType = type;
    node.leafValue = std::move(value);
    return node;
}

AstNode chain(std::vector<AstNode> children, std::vector<TokenType> ops)
{
    AstNode node;
    node.children = std::move(children);
    node.ops = std::move(ops);
    return node;
}

// Lex then parse; the parser is meaningless without the lexer, so exercise the
// real pipeline. Returns nullopt if either stage fails.
std::optional<ProbeAst> parse(std::string_view probe)
{
    std::optional<std::vector<Token>> tokens = lexProbe(probe);
    if (!tokens)
    {
        return std::nullopt;
    }
    return parseProbe(*tokens);
}

const std::string dbusA =
    "xyz.openbmc_project.FruDevice({'BUS': 6, 'ADDRESS': 80})";
const std::string dbusB = "xyz.openbmc_project.FruDevice({'BUS': 7})";

// Parse then fold through the shared evaluator. Leaves are only TRUE/FALSE, so
// no D-Bus is needed; this pins the evaluation ORDER independent of dbus
// probes.
std::optional<bool> eval(std::string_view probe)
{
    std::optional<ProbeAst> ast = parse(probe);
    if (!ast)
    {
        return std::nullopt;
    }
    return probe::evalAst(ast->root, [](const AstNode& node) {
        return node.leafType == TokenType::boolTrue;
    });
}

} // namespace

// A single operand parses to a bare leaf, not a one-child chain.
TEST(ParseProbe, SingleOperand)
{
    ProbeAst expected;
    expected.root = leaf(TokenType::boolTrue);
    EXPECT_EQ(parse("TRUE"), expected);

    expected.root = leaf(TokenType::found, "Board");
    EXPECT_EQ(parse("FOUND('Board')"), expected);

    expected.root = leaf(TokenType::dbusProbe, dbusA);
    EXPECT_EQ(parse(dbusA), expected);
}

// Backward compatibility: a flat mixed probe becomes one left-to-right chain,
// preserving the historic no-precedence semantics.
TEST(ParseProbe, FlatChainKeepsSourceOrder)
{
    ProbeAst expected;
    expected.root = chain(
        {leaf(TokenType::dbusProbe, dbusA), leaf(TokenType::dbusProbe, dbusB),
         leaf(TokenType::found, "Board")},
        {TokenType::opOr, TokenType::opAnd});
    EXPECT_EQ(parse(dbusA + " OR " + dbusB + " AND FOUND('Board')"), expected);
}

// A trailing MATCH_ONE is peeled off into the modifier flag, not the tree.
TEST(ParseProbe, TrailingMatchOne)
{
    ProbeAst expected;
    expected.root = chain(
        {leaf(TokenType::dbusProbe, dbusA), leaf(TokenType::dbusProbe, dbusB)},
        {TokenType::opOr});
    expected.matchOne = true;
    EXPECT_EQ(parse(dbusA + " OR " + dbusB + " MATCH_ONE"), expected);
}

// The feature: parentheses nest a sub-expression, overriding left-to-right.
TEST(ParseProbe, GroupingCreatesNestedChain)
{
    ProbeAst expected;
    expected.root =
        chain({leaf(TokenType::boolTrue),
               chain({leaf(TokenType::boolFalse), leaf(TokenType::boolTrue)},
                     {TokenType::opOr})},
              {TokenType::opAnd});
    EXPECT_EQ(parse("TRUE AND (FALSE OR TRUE)"), expected);
}

// Grouping must be able to change the shape relative to the flat form:
// "A OR B AND C" is one flat chain, but "A OR (B AND C)" nests B AND C.
TEST(ParseProbe, GroupingChangesStructure)
{
    ProbeAst flat;
    flat.root = chain({leaf(TokenType::boolTrue), leaf(TokenType::boolFalse),
                       leaf(TokenType::boolTrue)},
                      {TokenType::opOr, TokenType::opAnd});

    ProbeAst grouped;
    grouped.root =
        chain({leaf(TokenType::boolTrue),
               chain({leaf(TokenType::boolFalse), leaf(TokenType::boolTrue)},
                     {TokenType::opAnd})},
              {TokenType::opOr});

    EXPECT_EQ(parse("TRUE OR FALSE AND TRUE"), flat);
    EXPECT_EQ(parse("TRUE OR (FALSE AND TRUE)"), grouped);
    EXPECT_NE(parse("TRUE OR FALSE AND TRUE"),
              parse("TRUE OR (FALSE AND TRUE)"));
}

// Redundant parentheses around a single operand collapse to that operand.
TEST(ParseProbe, RedundantParensCollapse)
{
    EXPECT_EQ(parse("(TRUE)"), parse("TRUE"));
    EXPECT_EQ(parse("((FOUND('X')))"), parse("FOUND('X')"));
}

// Nested groups on both sides.
TEST(ParseProbe, DeeplyNested)
{
    ProbeAst expected;
    expected.root =
        chain({chain({leaf(TokenType::boolTrue), leaf(TokenType::boolFalse)},
                     {TokenType::opAnd}),
               chain({leaf(TokenType::boolFalse), leaf(TokenType::boolTrue)},
                     {TokenType::opOr})},
              {TokenType::opOr});
    EXPECT_EQ(parse("(TRUE AND FALSE) OR (FALSE OR TRUE)"), expected);
}

// Grammar errors all yield nullopt.
TEST(ParseProbe, Errors)
{
    EXPECT_EQ(parse("("), std::nullopt);                 // unmatched open
    EXPECT_EQ(parse(")"), std::nullopt);                 // unmatched close
    EXPECT_EQ(parse("TRUE)"), std::nullopt);             // trailing close
    EXPECT_EQ(parse("()"), std::nullopt);                // empty group
    EXPECT_EQ(parse("(TRUE"), std::nullopt);             // missing close
    EXPECT_EQ(parse("TRUE AND"), std::nullopt);          // operator, no operand
    EXPECT_EQ(parse("AND TRUE"), std::nullopt);          // leading operator
    EXPECT_EQ(parse("TRUE FALSE"), std::nullopt);        // two operands, no op
    EXPECT_EQ(parse("TRUE AND OR FALSE"), std::nullopt); // doubled operator
    EXPECT_EQ(parse("MATCH_ONE"), std::nullopt); // modifier, no expression
    EXPECT_EQ(parse("MATCH_ONE AND TRUE"), std::nullopt); // misplaced modifier
    EXPECT_EQ(parse("TRUE MATCH_ONE OR FALSE"), std::nullopt); // not last
    EXPECT_EQ(parse("(TRUE MATCH_ONE)"), std::nullopt); // modifier inside group
}

// A single operand folds to its own value.
TEST(EvalAst, SingleOperand)
{
    EXPECT_EQ(eval("TRUE"), true);
    EXPECT_EQ(eval("FALSE"), false);
}

// The flat form folds strictly left-to-right with no precedence. Under
// conventional precedence (AND tighter than OR) "TRUE OR FALSE AND FALSE" would
// be TRUE OR (FALSE AND FALSE) == true; left-to-right gives
// (TRUE OR FALSE) AND FALSE == false. This pins the no-precedence rule.
TEST(EvalAst, FlatChainFoldsLeftToRight)
{
    EXPECT_EQ(eval("TRUE OR FALSE AND FALSE"), false);
    EXPECT_EQ(eval("FALSE AND TRUE OR TRUE"), true);
}

// Grouping overrides the default order: the same operands flip their result
// when the AND is parenthesised.
TEST(EvalAst, GroupingOverridesOrder)
{
    EXPECT_EQ(eval("TRUE OR (FALSE AND FALSE)"), true);
    EXPECT_EQ(eval("(TRUE OR FALSE) AND FALSE"), false);
}

// Evaluation never short-circuits: every leaf is visited even once the running
// result is settled, so that in production all D-Bus operands run.
TEST(EvalAst, NoShortCircuitVisitsEveryLeaf)
{
    std::optional<ProbeAst> ast = parse("TRUE OR FALSE OR TRUE");
    ASSERT_TRUE(ast.has_value());
    int visited = 0;
    bool result = probe::evalAst(ast.value().root, [&](const AstNode& node) {
        ++visited;
        return node.leafType == TokenType::boolTrue;
    });
    EXPECT_TRUE(result);
    EXPECT_EQ(visited, 3);
}

// Grouping is recursive, so nesting beyond the parser's depth cap must be
// reported as a grammar error rather than overflowing the stack.
TEST(ParseProbe, RejectsDeeplyNestedGroups)
{
    std::string deep;
    deep.append(200, '(');
    deep += "TRUE";
    deep.append(200, ')');
    EXPECT_FALSE(parse(deep).has_value());

    // A sane nesting depth still parses.
    EXPECT_TRUE(parse("((((TRUE))))").has_value());
}
