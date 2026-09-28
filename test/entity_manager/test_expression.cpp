#include "entity_manager/expression.hpp"

#include <optional>
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

    EXPECT_EQ(expression::evaluate(3, tokens.begin(), end), std::nullopt);
}

TEST(Expression, RejectsNonNumericOperand)
{
    std::vector<std::string> tokens = {"+", "junk"};
    auto end = tokens.end();

    EXPECT_EQ(expression::evaluate(3, tokens.begin(), end), std::nullopt);
}

TEST(Expression, RejectsOutOfRangeOperand)
{
    std::vector<std::string> tokens = {"+", "9999999999999999999999999"};
    auto end = tokens.end();

    EXPECT_EQ(expression::evaluate(3, tokens.begin(), end), std::nullopt);
}

TEST(Expression, RejectsDivisionByZero)
{
    std::vector<std::string> tokens = {"/", "0"};
    auto end = tokens.end();

    EXPECT_EQ(expression::evaluate(3, tokens.begin(), end), std::nullopt);
}

TEST(Expression, RejectsModuloByZero)
{
    EXPECT_EQ(expression::evaluate(3, expression::Operation::modulo, 0),
              std::nullopt);
}

TEST(Expression, EvaluatesArithmeticOperations)
{
    EXPECT_EQ(expression::evaluate(3, expression::Operation::addition, -3), 0);
    EXPECT_EQ(expression::evaluate(3, expression::Operation::subtraction, 5),
              -2);
    EXPECT_EQ(expression::evaluate(3, expression::Operation::multiplication, 2),
              6);
    EXPECT_EQ(expression::evaluate(6, expression::Operation::division, 2), 3);
    EXPECT_EQ(expression::evaluate(7, expression::Operation::modulo, 2), 1);
}

TEST(Expression, PropagatesFailureAfterValidOperation)
{
    std::vector<std::string> tokens = {"+", "2", "/", "0"};
    auto end = tokens.end();

    EXPECT_EQ(expression::evaluate(3, tokens.begin(), end), std::nullopt);
    EXPECT_EQ(end, tokens.end());
}

TEST(Expression, StopsBeforeLiteralSuffix)
{
    std::vector<std::string> tokens = {"-", "3", "sensor"};
    auto end = tokens.end();

    EXPECT_EQ(expression::evaluate(3, tokens.begin(), end), 0);
    EXPECT_EQ(end, tokens.begin() + 2);
}
