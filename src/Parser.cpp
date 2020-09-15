#include <boost/config/warning_disable.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/spirit/home/x3/support/ast/variant.hpp>

#include <iostream>
#include <list>
#include <numeric>
#include <string>
#include <variant>

namespace client::ast
{

// forward declarations so we can use them recursively.
struct Signed;
struct NumericOp;

// Ideally we'd use std::variant, but we need something that supports forward
// declared variables
using Operand =
    boost::spirit::x3::variant<int, boost::spirit::x3::forward_ast<Signed>,
                               boost::spirit::x3::forward_ast<NumericOp>>;

struct Signed
{
    // A signed value.  Sign is - or +  ex -1
    char sign;
    Operand operandElement;
};

struct Operation
{
    // An operation (+ - * /) and a right hand side operand to apply it to.
    char operatorChar;
    Operand operandElement;
};

struct NumericOp
{
    // NumericOp represents a list of operands, and operations to be performaned
    // on each one.  Note: this datastructure is not in operator priority order
    Operand first;
    std::vector<Operation> rest;
};

struct Variable
{
    int expr;
};

using StringPieceType =
    std::list<boost::spirit::x3::variant<std::string, NumericOp>>;
struct StringPiece
{
    // String pieces represent a list of input pieces that may or may not
    // require evaluation.  NumericOp objects require an eval, std::string does
    // not.
    StringPieceType expr;
};

} // namespace client::ast

// Turn these structs into full tuples.  The obfuscation isn't great here, but
// it allows boost spirit to see into the structures
BOOST_FUSION_ADAPT_STRUCT(client::ast::Signed, sign, operandElement);
BOOST_FUSION_ADAPT_STRUCT(client::ast::Operation, operatorChar, operandElement);
BOOST_FUSION_ADAPT_STRUCT(client::ast::NumericOp, first, rest);
BOOST_FUSION_ADAPT_STRUCT(client::ast::Variable, expr);
BOOST_FUSION_ADAPT_STRUCT(client::ast::StringPiece, expr);

namespace client::ast
{

struct Eval
{
    std::optional<int> calculateOperator(int lhs, Operation const& x) const
    {
        std::optional<int> rhs = boost::apply_visitor(*this, x.operandElement);
        if (!rhs)
        {
            return rhs;
        }
        switch (x.operatorChar)
        {
            case '+':
                return lhs + *rhs;
            case '-':
                return lhs - *rhs;
            case '*':
                return lhs * *rhs;
            case '/':
                return lhs / *rhs;
            case '%':
                return lhs % *rhs;
        }
        return std::nullopt;
    }

    std::optional<int> operator()(unsigned int n) const
    {
        return n;
    }

    std::optional<int> operator()(Signed const& x) const
    {
        std::optional<int> rhs = boost::apply_visitor(*this, x.operandElement);
        if (!rhs)
        {
            return rhs;
        }
        switch (x.sign)
        {
            case '-':
                return -(*rhs);
            case '+':
                return *rhs;
        }
        return std::nullopt;
    }

    std::optional<int> operator()(NumericOp const& x) const
    {
        std::optional<int> r = boost::apply_visitor(*this, x.first);
        if (!r)
        {
            return std::nullopt;
        }
        int out = *r;

        for (const ast::Operation& element : x.rest)
        {
            std::optional<int> r2 = calculateOperator(out, element);
            if (!r2)
            {
                return std::nullopt;
            }
            out = *r2;
        }

        return out;
    }

    std::optional<std::string> evaluate(ast::StringPiece const& x) const
    {
        std::string out;
        for (const auto& value : x.expr)
        {
            std::optional<std::string> ret = boost::apply_visitor(
                [this](const auto& v) -> std::optional<std::string> {
                    if constexpr (std::is_same<std::decay_t<decltype(v)>,
                                               std::string>::value)
                    {
                        return v;
                    }
                    else
                    {
                        std::optional<int> ret = (*this)(v);
                        if (!ret)
                        {
                            return std::nullopt;
                        }
                        return std::to_string(*ret);
                    }
                },
                value);

            if (!ret)
            {
                return std::nullopt;
            }
            out += *ret;
        }
        return out;
    }
};

} // namespace client::ast

namespace client
{
namespace entity_template_grammar
{

namespace x3 = boost::spirit::x3;

struct SymbolsType_ : x3::symbols<int>
{
    SymbolsType_()
    {
        add("ADDRESS", 81);
    }
} symbols;

// Non function strings go straight through
x3::rule<class stringPiece, ast::StringPieceType> const stringPiece = "stringPiece";
x3::rule<class expression, ast::NumericOp> const expression = "expression";
x3::rule<class term, ast::NumericOp> const term = "term";
x3::rule<class factor, ast::Operand> const factor = "factor";
x3::rule<class stringOp, std::string> const stringOp = "stringOp";
x3::rule<class variableOp, ast::Variable> const variableOp = "variableOp";

auto const alpha = +(x3::ascii::space | x3::ascii::upper | x3::ascii::lower);

auto const stringPiece_def = +(expression | stringOp);
auto const variableOp_def = x3::omit[x3::char_('$')] >> symbols;

auto const ignoreSpace = x3::omit[*x3::ascii::space];

auto const stringOp_def = x3::lexeme[alpha];

auto const
    expression_def = term >>
                     *((ignoreSpace >> x3::char_('+') >> ignoreSpace >> term) |
                       (ignoreSpace >> x3::char_('-') >> ignoreSpace >> term));

auto const
    term_def = factor >>
               *((ignoreSpace >> x3::char_('*') >> ignoreSpace >> factor) |
                 (ignoreSpace >> x3::char_('/') >> ignoreSpace >> factor) |
                 (ignoreSpace >> x3::char_('%') >> ignoreSpace >> factor));

auto const factor_def = x3::uint_ | variableOp | '(' >> expression >> ')' |
                        (x3::char_('-') >> factor) | (x3::char_('+') >> factor);

BOOST_SPIRIT_DEFINE(stringPiece, expression, term, factor, stringOp,
                    variableOp);

} // namespace entity_template_grammar

using entity_template_grammar::stringPiece;

} // namespace client

int main()
{
    std::string str;
    while (std::getline(std::cin, str))
    {
        if (str.empty() || str[0] == 'q' || str[0] == 'Q')
            break;

        client::ast::StringPiece ast;

        std::string::const_iterator iter = str.begin();
        std::string::const_iterator end = str.end();

        bool r = boost::spirit::x3::parse(iter, end, client::stringPiece, ast);

        if (r && iter == end)
        {
            client::ast::Eval eval;
            std::optional<std::string> result = eval.evaluate(ast);
            if (!result)
            {
                result = "Failed";
            }
            std::cout << "\nResult: " << *result << "\n";
        }
        else
        {
            std::string rest(iter, end);
            std::cout << "Parsing failed\n";
            std::cout << "stopped at: \"" << rest << "\"\n";
        }
    }

    return 0;
}
