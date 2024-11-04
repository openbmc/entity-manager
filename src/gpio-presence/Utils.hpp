/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024. All rights
 * reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

// some common code from dbus-sensors

#include <boost/asio.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/type_index.hpp>
#include <gpiod.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>
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

using BasicVariantType =
    std::variant<std::vector<std::string>, std::string, int64_t, uint64_t,
                 double, int32_t, uint32_t, int16_t, uint16_t, uint8_t, bool>;
using SensorBaseConfigMap =
    boost::container::flat_map<std::string, BasicVariantType>;
using SensorBaseConfiguration = std::pair<std::string, SensorBaseConfigMap>;
using SensorData = boost::container::flat_map<std::string, SensorBaseConfigMap>;
using ManagedObjectType =
    boost::container::flat_map<sdbusplus::message::object_path, SensorData>;

template <typename T>
inline T loadVariant(const SensorBaseConfigMap& data, const std::string& key)
{
    auto it = data.find(key);
    if (it == data.end())
    {
        std::cerr << "Configuration missing " << key << "\n";
        throw std::invalid_argument("Key Missing");
    }
    if constexpr (std::is_same_v<T, double>)
    {
        return std::visit(VariantToDoubleVisitor(), it->second);
    }
    else if constexpr (std::is_unsigned_v<T>)
    {
        return std::visit(VariantToUnsignedIntVisitor(), it->second);
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        return std::visit(VariantToStringVisitor(), it->second);
    }
    else
    {
        static_assert(!std::is_same_v<T, T>, "Type Not Implemented");
    }
}
