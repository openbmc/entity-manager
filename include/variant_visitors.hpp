/*
// Copyright (c) 2019 Intel Corporation
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
/// \file variant_visitors.hpp

#pragma once
#include <boost/type_index.hpp>

#include <stdexcept>
#include <string>
#include <variant>

namespace details
{
template <typename U>
struct VariantToNumericVisitor
{
    template <typename T>
    U operator()(const T& t) const
    {
        if constexpr (std::is_arithmetic_v<T>)
        {
            return static_cast<U>(t);
        }
        throw std::invalid_argument(
            "Cannot translate type " +
            boost::typeindex::type_id<T>().pretty_name() + " to " +
            boost::typeindex::type_id<U>().pretty_name());
    }
};

} // namespace details
using VariantToFloatVisitor = details::VariantToNumericVisitor<float>;
using VariantToIntVisitor = details::VariantToNumericVisitor<int>;
using VariantToUnsignedIntVisitor =
    details::VariantToNumericVisitor<unsigned int>;
using VariantToDoubleVisitor = details::VariantToNumericVisitor<double>;

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
        throw std::invalid_argument(
            "Cannot translate type " +
            boost::typeindex::type_id<T>().pretty_name() + " to string");
    }
};
