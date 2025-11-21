#pragma once

#include "utils.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using ::testing::Contains;
using ::testing::Eq;
using ::testing::FieldsAre;
using ::testing::StrEq;
using ::testing::VariantWith;

template <typename T>
void assertHasProperty(const DBusInterface& value, const std::string& name,
                       const T& expected)
{
    EXPECT_THAT(value,
                Contains(FieldsAre(StrEq(name), VariantWith<T>(Eq(expected)))));
};
