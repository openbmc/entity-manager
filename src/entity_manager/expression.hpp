// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2017 Intel Corporation, 2022 IBM Corp.

#pragma once

#include <optional>
#include <string>
#include <vector>

namespace expression
{
enum class Operation
{
    addition,
    division,
    multiplication,
    subtraction,
    modulo,
};

std::optional<Operation> parseOperation(const std::string& op);
int evaluate(int a, Operation op, int b);
int evaluate(int substitute, std::vector<std::string>::iterator curr,
             std::vector<std::string>::iterator& end);
} // namespace expression
