// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2019 Intel Corporation

#pragma once
#include <stdexcept>
#include <string>
#include <variant>

struct VariantToIntVisitor
{
    template <typename T>
    int operator()(const T& t) const
    {
        if constexpr (std::is_arithmetic_v<T>)
        {
            return static_cast<int>(t);
        }
        throw std::invalid_argument("Cannot translate type to int");
    }
};

struct VariantToStringVisitor
{
    template <typename T>
    std::string operator()(const T& t) const
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            return t;
        }
        else if constexpr (std::is_arithmetic_v<T>)
        {
            return std::to_string(t);
        }
        throw std::invalid_argument("Cannot translate type to string");
    }
};
