#include "entity_manager/probe_lexer.hpp"

#include <string>
#include <vector>

#include <gtest/gtest.h>

using probe::lexProbe;
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

// A D-Bus probe is kept as one opaque token holding the full statement text.
TEST(LexProbe, DbusProbe)
{
    const std::string dbus =
        "xyz.openbmc_project.FruDevice({'BUS': 6, 'ADDRESS': 80})";
    EXPECT_EQ(lexProbe(dbus),
              (std::vector<Token>{{TokenType::dbusProbe, dbus}}));
}

// The regression this change fixes: a keyword appearing as a substring inside a
// D-Bus probe must not be tokenized as an operator.
TEST(LexProbe, KeywordSubstringInsideDbusProbeIsNotAnOperator)
{
    const std::string dbus =
        "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'HORIZON'})";
    EXPECT_EQ(lexProbe(dbus),
              (std::vector<Token>{{TokenType::dbusProbe, dbus}}));
}

// The mixed operator pattern used by several platforms:
// A OR B AND FOUND('X').
TEST(LexProbe, MixedOperators)
{
    const std::string a = "xyz.openbmc_project.FruDevice({'BUS': 6})";
    const std::string b = "xyz.openbmc_project.FruDevice({'BUS': 7})";
    EXPECT_EQ(lexProbe(a + " OR " + b + " AND FOUND('Board')"),
              (std::vector<Token>{{TokenType::dbusProbe, a},
                                  {TokenType::opOr, ""},
                                  {TokenType::dbusProbe, b},
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
    EXPECT_EQ(lexProbe(dbus),
              (std::vector<Token>{{TokenType::dbusProbe, dbus}}));
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
    // A stray delimiter that does not start a token.
    EXPECT_EQ(lexProbe("{'A': 1}"), std::nullopt);
    // Unterminated quote (keeps the paren depth open to end of input).
    EXPECT_EQ(lexProbe("FOUND('unterminated"), std::nullopt);
    EXPECT_EQ(lexProbe("xyz.Iface({'A': 'unterminated})"), std::nullopt);
}
