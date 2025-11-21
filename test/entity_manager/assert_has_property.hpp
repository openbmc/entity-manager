#pragma once

#include "dbus_test.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include <gtest/gtest.h>

template <typename T>
void assertHasProperty(const DBusPropertiesMap& value, const std::string& name,
                       const T& expected)
{
    if (!value.contains(name))
    {
        lg2::error("test: property '{NAME}' not found", "NAME", name);
    }
    ASSERT_TRUE(value.contains(name));

    if (!std::holds_alternative<T>(std::get<1>(*value.find(name))))
    {
        lg2::error("test: property '{NAME}' had unexpected type", "NAME", name);
    }
    ASSERT_TRUE(std::holds_alternative<T>(std::get<1>(*value.find(name))));

    const auto actual = std::get<T>(std::get<1>(*value.find(name)));

    EXPECT_EQ(actual, expected);
};
