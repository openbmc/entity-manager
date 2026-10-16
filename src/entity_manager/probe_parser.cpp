#include "probe_parser.hpp"

#include "probe_lexer.hpp"

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace probe
{

namespace
{

// Recursive-descent parser for the probe grammar (see docs/probe_grammar.md):
//
//   probe = expr [ "MATCH_ONE" ]
//   expr  = term { ( "AND" | "OR" ) term }
//   term  = operand | "(" expr ")"
//
// expr combines terms strictly left-to-right with no operator precedence: the
// chain built by parseExpr keeps operands in source order so the evaluator can
// fold them left-to-right, exactly as the historic flat evaluator did.
// Parentheses are the only way to nest, and hence the only way to change the
// default order.
class Parser
{
  public:
    explicit Parser(const std::vector<Token>& tokensIn) : tokens(tokensIn) {}

    std::optional<ProbeAst> parse()
    {
        ProbeAst result;

        // A trailing MATCH_ONE is a whole-probe modifier, not an operand; peel
        // it off before parsing the boolean expression.
        end = tokens.size();
        if (end > 0 && tokens[end - 1].type == TokenType::matchOne)
        {
            result.matchOne = true;
            --end;
        }

        // MATCH_ONE anywhere but the very end is a grammar error.
        for (size_t j = 0; j < end; ++j)
        {
            if (tokens[j].type == TokenType::matchOne)
            {
                return std::nullopt;
            }
        }

        std::optional<AstNode> root = parseExpr();
        if (!root || pos != end)
        {
            // Parse failed, or tokens remain (e.g. an unmatched ')').
            return std::nullopt;
        }
        result.root = std::move(*root);
        return result;
    }

  private:
    const std::vector<Token>& tokens;
    size_t pos = 0;
    size_t end = 0;
    size_t depth = 0;

    // Bound on grouping nesting. Groups recurse (parseTerm -> parseExpr), so
    // this caps parse recursion and, because the AST is no deeper than the
    // groups that built it, evalAst recursion too: a pathologically nested
    // probe yields a grammar error instead of overflowing the stack.
    static constexpr size_t maxDepth = 128;

    const Token* peek() const
    {
        return pos < end ? &tokens[pos] : nullptr;
    }

    // expr = term { ( "AND" | "OR" ) term }
    std::optional<AstNode> parseExpr()
    {
        std::optional<AstNode> first = parseTerm();
        if (!first)
        {
            return std::nullopt;
        }

        AstNode node;
        node.children.push_back(std::move(*first));

        while (const Token* t = peek())
        {
            if (t->type != TokenType::opAnd && t->type != TokenType::opOr)
            {
                break;
            }
            TokenType op = t->type;
            ++pos; // consume the operator
            std::optional<AstNode> rhs = parseTerm();
            if (!rhs)
            {
                // An operator with no operand after it.
                return std::nullopt;
            }
            node.ops.push_back(op);
            node.children.push_back(std::move(*rhs));
        }

        // A lone term is returned directly rather than wrapped in a 1-child
        // chain, so a bare operand and a redundant "(operand)" produce the same
        // AST.
        if (node.children.size() == 1)
        {
            return std::move(node.children.front());
        }
        return node;
    }

    // term = operand | "(" expr ")"
    std::optional<AstNode> parseTerm()
    {
        const Token* t = peek();
        if (t == nullptr)
        {
            return std::nullopt;
        }

        if (t->type == TokenType::lparen)
        {
            ++pos; // consume '('
            if (depth >= maxDepth)
            {
                return std::nullopt;
            }
            ++depth;
            std::optional<AstNode> inner = parseExpr();
            --depth;
            if (!inner)
            {
                return std::nullopt;
            }
            const Token* close = peek();
            if (close == nullptr || close->type != TokenType::rparen)
            {
                return std::nullopt; // missing ')'
            }
            ++pos;                   // consume ')'
            return inner;
        }

        switch (t->type)
        {
            case TokenType::boolTrue:
            case TokenType::boolFalse:
            case TokenType::found:
            case TokenType::dbusProbe:
            {
                AstNode leaf;
                leaf.leafType = t->type;
                leaf.leafValue = t->value;
                ++pos;
                return leaf;
            }
            default:
                // An operator, a ')', or a MATCH_ONE where an operand was
                // expected.
                return std::nullopt;
        }
    }
};

} // namespace

std::optional<ProbeAst> parseProbe(const std::vector<Token>& tokens)
{
    return Parser(tokens).parse();
}

} // namespace probe
