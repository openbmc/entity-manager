/*
// Copyright (c) 2017 Intel Corporation
// Copyright (c) 2022 IBM Corp.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "Expression.hpp"

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
            return a / b;
        }
        case Operation::modulo:
        {
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
