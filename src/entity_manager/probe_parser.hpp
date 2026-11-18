#pragma once

#include "probe_lexer.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace probe
{

// AND and OR have the same precedence and associate to the left, so the left
// child of an operator is everything before it.
struct Node
{
    Token token;
    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;
};

struct Ast
{
    std::unique_ptr<Node> root;
    // MATCH_ONE applies to the whole result, not to an operand.
    bool matchOne = false;
};

// Returns std::nullopt when the tokens are not a Probe.
std::optional<Ast> parseProbe(const std::vector<Token>& tokens);

// A Probe that follows the grammar: the tokens it was read from and their tree.
struct ParsedProbe
{
    std::vector<Token> tokens;
    Ast ast;
};

// Parse validated probe statements. The statements are joined with single
// spaces and lexed. Returns std::nullopt on a lexing error or when the tokens
// do not follow the grammar.
std::optional<ParsedProbe> parseProbeCommand(
    const std::vector<std::string>& probeField);

} // namespace probe
