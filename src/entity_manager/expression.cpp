// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2017 Intel Corporation, 2022 IBM Corp.

#include "expression.hpp"

#include <phosphor-logging/lg2.hpp>

#include <stdexcept>

namespace expression
{
std::optional<Operation> parseOperation(std::string& op)
{
    if (op == "+")
    {
        return Operation::addition;
    }
    if (op == "-")
    {
        return Operation::subtraction;
    }
    if (op == "*")
    {
        return Operation::multiplication;
    }
    if (op == R"(%)")
    {
        return Operation::modulo;
    }
    if (op == R"(/)")
    {
        return Operation::division;
    }

    return std::nullopt;
}

std::optional<int> evaluate(int a, Operation op, int b)
{
    switch (op)
    {
        case Operation::addition:
        {
            return a + b;
        }
        case Operation::subtraction:
        {
            return a - b;
        }
        case Operation::multiplication:
        {
            return a * b;
        }
        case Operation::division:
        {
            if (b == 0)
            {
                lg2::error("Math error: Attempted to divide by zero");
                return std::nullopt;
            }
            return a / b;
        }
        case Operation::modulo:
        {
            if (b == 0)
            {
                lg2::error("Math error: Attempted to divide by zero");
                return std::nullopt;
            }
            return a % b;
        }

        default:
            lg2::error("Unrecognised template expression operation");
            return std::nullopt;
    }
}

static std::optional<int> parseOperand(const std::string& operand)
{
    size_t parsed = 0;
    int constant = 0;
    try
    {
        constant = std::stoi(operand, &parsed);
    }
    catch (const std::invalid_argument&)
    {
        lg2::error("Invalid template expression operand {STR}", "STR", operand);
        return std::nullopt;
    }
    catch (const std::out_of_range&)
    {
        lg2::error("Template expression operand out of range {STR}", "STR",
                   operand);
        return std::nullopt;
    }
    if (parsed != operand.size())
    {
        lg2::error("Invalid template expression operand {STR}", "STR", operand);
        return std::nullopt;
    }
    return constant;
}

std::optional<int> evaluate(int substitute,
                            std::vector<std::string>::iterator curr,
                            std::vector<std::string>::iterator& end)
{
    bool isOperator = true;
    std::optional<Operation> next = Operation::addition;

    for (; curr != end; curr++)
    {
        if (isOperator)
        {
            next = expression::parseOperation(*curr);
            if (!next)
            {
                break;
            }
        }
        else
        {
            auto constant = parseOperand(*curr);
            if (!constant)
            {
                return std::nullopt;
            }
            auto result = evaluate(substitute, *next, *constant);
            if (!result)
            {
                return std::nullopt;
            }
            substitute = *result;
        }
        isOperator = !isOperator;
    }

    end = curr;
    return substitute;
}
} // namespace expression
