#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace probe
{

// The kinds of tokens a "Probe" statement can be broken into. See
// docs/probe_grammar.md for the grammar and evaluation semantics.
enum class TokenType
{
    boolTrue,  // TRUE
    boolFalse, // FALSE
    opAnd,     // AND
    opOr,      // OR
    matchOne,  // MATCH_ONE
    found,     // FOUND('name'); value holds the name
    dbusProbe, // iface({...});  value holds the full "iface({...})" text
};

struct Token
{
    TokenType type;
    // For 'found', the probe name; for 'dbusProbe', the full statement text;
    // empty for the keyword tokens.
    std::string value;

    bool operator==(const Token&) const = default;
};

// Tokenize a probe expression. The caller joins a "Probe" array's statements
// with single spaces first, so a single string is always passed here. Returns
// std::nullopt on a syntax error.
std::optional<std::vector<Token>> lexProbe(std::string_view probe);

} // namespace probe
