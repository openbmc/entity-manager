#include "probe_parser.hpp"

#include "probe_lexer.hpp"

#include <phosphor-logging/lg2.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace probe
{

namespace
{

class Parser
{
  public:
    explicit Parser(const std::vector<Token>& tokens) : tokens(tokens) {}

    std::optional<Ast> parse();

  private:
    const std::vector<Token>& tokens;
    size_t position = 0;

    std::unique_ptr<Node> parseExpression();
    std::unique_ptr<Node> parseOperand();
};

std::optional<Ast> Parser::parse()
{
    Ast ast;
    ast.root = parseExpression();
    if (!ast.root)
    {
        return std::nullopt;
    }

    if (position < tokens.size() &&
        tokens[position].type == TokenType::matchOne)
    {
        ast.matchOne = true;
        ++position;
    }

    // Anything left over is a second MATCH_ONE or tokens after it, or an
    // operand with no operator before it.
    if (position != tokens.size())
    {
        return std::nullopt;
    }
    return ast;
}

std::unique_ptr<Node> Parser::parseExpression()
{
    std::unique_ptr<Node> left = parseOperand();

    while (left && position < tokens.size() &&
           (tokens[position].type == TokenType::opAnd ||
            tokens[position].type == TokenType::opOr))
    {
        const Token& op = tokens[position++];
        std::unique_ptr<Node> right = parseOperand();
        if (!right)
        {
            return nullptr;
        }
        left =
            std::make_unique<Node>(Node{op, std::move(left), std::move(right)});
    }
    return left;
}

std::unique_ptr<Node> Parser::parseOperand()
{
    if (position == tokens.size())
    {
        return nullptr;
    }

    const Token& token = tokens[position];
    switch (token.type)
    {
        case TokenType::boolTrue:
        case TokenType::boolFalse:
        case TokenType::found:
        case TokenType::dbusProbe:
            ++position;
            return std::make_unique<Node>(Node{token, nullptr, nullptr});
        case TokenType::opAnd:
        case TokenType::opOr:
        case TokenType::matchOne:
            break;
    }
    return nullptr;
}

} // namespace

std::optional<Ast> parseProbe(const std::vector<Token>& tokens)
{
    return Parser(tokens).parse();
}

std::optional<ParsedProbe> parseProbeCommand(
    const std::vector<std::string>& probeField)
{
    // Join the statements (array form) into a single string, then lex it.
    std::string joined;
    for (const auto& probe : probeField)
    {
        if (!joined.empty())
        {
            joined += ' ';
        }
        joined += probe;
    }

    std::optional<std::vector<Token>> tokens = lexProbe(joined);
    if (!tokens)
    {
        lg2::error("Probe statement failed to parse: {PROBE}", "PROBE", joined);
        return std::nullopt;
    }
    if (tokens->empty())
    {
        lg2::error("Probe statement produced no tokens: {PROBE}", "PROBE",
                   joined);
        return std::nullopt;
    }
    // A statement with a missing or extra operator has no tree. Rejecting it
    // here lets the rest of the code assume a well-formed Probe.
    std::optional<Ast> ast = parseProbe(*tokens);
    if (!ast)
    {
        lg2::error("Probe statement does not follow the grammar: {PROBE}",
                   "PROBE", joined);
        return std::nullopt;
    }
    return ParsedProbe{std::move(*tokens), std::move(*ast)};
}

} // namespace probe
