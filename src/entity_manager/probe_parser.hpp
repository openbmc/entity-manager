#pragma once

#include "probe_lexer.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace probe
{

// An abstract syntax tree for a parsed "Probe" expression. A node is either:
//  - a leaf operand (TRUE / FALSE / FOUND / dbusProbe), or
//  - a chain: two or more child terms combined left-to-right by AND/OR
//    operators. There is intentionally NO operator precedence (see
//    docs/probe_grammar.md); parentheses in the source produce nested chain
//    nodes, which is the only way to override the default left-to-right order.
struct AstNode
{
    // Leaf payload (valid when isLeaf()).
    TokenType leafType = TokenType::boolFalse;
    std::string leafValue; // FOUND name, or the full "iface({...})" text.

    // Chain payload: children[0] ops[0] children[1] ops[1] children[2] ...
    // ops.size() == children.size() - 1; each op is opAnd or opOr.
    std::vector<AstNode> children;
    std::vector<TokenType> ops;

    bool isLeaf() const
    {
        return children.empty();
    }

    bool operator==(const AstNode&) const = default;
};

// The result of parsing a probe token stream.
struct ProbeAst
{
    AstNode root;
    bool matchOne = false; // a trailing MATCH_ONE modifier was present.

    bool operator==(const ProbeAst&) const = default;
};

// Parse a lexed probe into an AST. Returns std::nullopt on a grammar error:
// unbalanced parentheses, an operator with no operand, an empty group, trailing
// tokens, or a misplaced MATCH_ONE (it may only appear as the final token).
std::optional<ProbeAst> parseProbe(const std::vector<Token>& tokens);

// Evaluate an AST by folding each chain strictly left-to-right with NO operator
// precedence and NO short-circuit (see docs/probe_grammar.md): 'evalLeaf' is
// invoked once per leaf operand, in source order, and returns that operand's
// boolean value. Grouping is honoured implicitly by the tree structure. Keeping
// the fold pure (leaf semantics live in the caller's 'evalLeaf') lets the
// evaluation order be unit-tested without any D-Bus dependency.
template <typename EvalLeaf>
bool evalAst(const AstNode& node, EvalLeaf&& evalLeaf)
{
    if (node.isLeaf())
    {
        return evalLeaf(node);
    }
    bool result = evalAst(node.children[0], evalLeaf);
    for (size_t i = 0; i < node.ops.size(); ++i)
    {
        bool rhs = evalAst(node.children[i + 1], evalLeaf);
        result = (node.ops[i] == TokenType::opAnd) ? (result && rhs)
                                                   : (result || rhs);
    }
    return result;
}

} // namespace probe
