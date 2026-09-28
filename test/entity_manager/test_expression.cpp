#include "entity_manager/expression.hpp"

#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

TEST(Expression, EvaluatesValidOperands)
{
    std::vector<std::string> tokens = {"+", "2", "*", "3"};
    auto end = tokens.end();

    EXPECT_EQ(expression::evaluate(1, tokens.begin(), end), 9);
    EXPECT_EQ(end, tokens.end());
}

TEST(Expression, RejectsTrailingCharactersInOperand)
{
    std::vector<std::string> tokens = {"+", "2junk", "*", "3"};
    auto end = tokens.end();

    EXPECT_THROW(expression::evaluate(3, tokens.begin(), end),
                 std::invalid_argument);
}

TEST(Expression, RejectsNonNumericOperand)
{
    std::vector<std::string> tokens = {"+", "junk"};
    auto end = tokens.end();

    EXPECT_THROW(expression::evaluate(3, tokens.begin(), end),
                 std::invalid_argument);
}

TEST(Expression, RejectsOutOfRangeOperand)
{
    std::vector<std::string> tokens = {"+", "9999999999999999999999999"};
    auto end = tokens.end();

    EXPECT_THROW(expression::evaluate(3, tokens.begin(), end),
                 std::out_of_range);
}

TEST(Expression, RejectsDivisionByZero)
{
    std::vector<std::string> tokens = {"/", "0"};
    auto end = tokens.end();

    EXPECT_THROW(expression::evaluate(3, tokens.begin(), end),
                 std::runtime_error);
}
