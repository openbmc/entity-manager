// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2017 Intel Corporation, 2022 IBM Corp.

#include "expression.hpp"

#include <iostream>
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

int evaluate(int a, Operation op, int b)
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
                throw std::runtime_error(
                    "Math error: Attempted to divide by Zero\n");
            }
            return a / b;
        }
        case Operation::modulo:
        {
            if (b == 0)
            {
                throw std::runtime_error(
                    "Math error: Attempted to divide by Zero\n");
            }
            return a % b;
        }

        default:
            throw std::invalid_argument("Unrecognised operation");
    }
}

int evaluate(int substitute, std::vector<std::string>::iterator curr,
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
            try
            {
                int constant = std::stoi(*curr);
                substitute = evaluate(substitute, *next, constant);
            }
            catch (const std::invalid_argument&)
            {
                std::cerr << "Parameter not supported for templates " << *curr
                          << "\n";
                continue;
            }
        }
        isOperator = !isOperator;
    }

    end = curr;
    return substitute;
}
} // namespace expression
