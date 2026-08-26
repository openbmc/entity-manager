#include "probe_lexer.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace probe
{

namespace
{

constexpr bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

// Characters allowed in a bare keyword or an interface name.
constexpr bool isNameChar(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '.';
}

constexpr std::array keywords{
    std::pair<std::string_view, TokenType>{"TRUE", TokenType::boolTrue},
    std::pair<std::string_view, TokenType>{"FALSE", TokenType::boolFalse},
    std::pair<std::string_view, TokenType>{"AND", TokenType::opAnd},
    std::pair<std::string_view, TokenType>{"OR", TokenType::opOr},
    std::pair<std::string_view, TokenType>{"MATCH_ONE", TokenType::matchOne},
};

std::optional<TokenType> keywordType(std::string_view word)
{
    for (const auto& [text, type] : keywords)
    {
        if (word == text)
        {
            return type;
        }
    }
    return std::nullopt;
}

// Given the index of an opening '(', return the index just past the matching
// ')', tracking nested () / {} and ignoring delimiters inside quotes. Returns
// npos if unbalanced.
size_t findBalancedEnd(std::string_view s, size_t open)
{
    int depth = 0;
    char quote = '\0';
    for (size_t i = open; i < s.size(); ++i)
    {
        char c = s[i];
        if (quote != '\0')
        {
            if (c == '\\' && i + 1 < s.size())
            {
                // Skip the escaped character (e.g. \' inside a value) so it
                // does not prematurely close the quote.
                ++i;
            }
            else if (c == quote)
            {
                quote = '\0';
            }
            continue;
        }
        switch (c)
        {
            case '\'':
            case '"':
                quote = c;
                break;
            case '(':
            case '{':
                ++depth;
                break;
            case ')':
            case '}':
                --depth;
                if (depth == 0)
                {
                    return i + 1;
                }
                break;
            default:
                break;
        }
    }
    // Ran off the end with parens still open, or a quote never closed.
    return std::string_view::npos;
}

// Extract the name from a FOUND('...') argument list, i.e. the text between the
// outer parentheses with surrounding whitespace and single quotes stripped.
std::string extractFoundName(std::string_view args)
{
    // args includes the surrounding parentheses: ('...')
    std::string_view inner = args.substr(1, args.size() - 2);
    size_t begin = inner.find_first_not_of(" \t'");
    if (begin == std::string_view::npos)
    {
        return {};
    }
    size_t end = inner.find_last_not_of(" \t'");
    return std::string(inner.substr(begin, end - begin + 1));
}

} // namespace

std::optional<std::vector<Token>> lexProbe(std::string_view probe)
{
    std::vector<Token> tokens;
    size_t i = 0;
    while (i < probe.size())
    {
        if (isSpace(probe[i]))
        {
            ++i;
            continue;
        }

        // Read a name run: a keyword, FOUND, or an interface name.
        size_t start = i;
        while (i < probe.size() && isNameChar(probe[i]))
        {
            ++i;
        }
        if (i == start)
        {
            // Unexpected character that does not start a token.
            return std::nullopt;
        }
        std::string_view word = probe.substr(start, i - start);

        // A call: the name is immediately followed by '('.
        if (i < probe.size() && probe[i] == '(')
        {
            size_t end = findBalancedEnd(probe, i);
            if (end == std::string_view::npos)
            {
                return std::nullopt;
            }
            if (word == "FOUND")
            {
                tokens.push_back({TokenType::found,
                                  extractFoundName(probe.substr(i, end - i))});
            }
            else
            {
                tokens.push_back(
                    {TokenType::dbusProbe,
                     std::string(probe.substr(start, end - start))});
            }
            i = end;
            continue;
        }

        // Otherwise the name must be a bare keyword.
        std::optional<TokenType> kw = keywordType(word);
        if (!kw)
        {
            return std::nullopt;
        }
        tokens.push_back({*kw, std::string{}});
    }

    return tokens;
}

} // namespace probe
