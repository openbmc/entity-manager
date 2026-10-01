#include "entity_manager/perform_probe.hpp"

#include <string>
#include <vector>

#include <gtest/gtest.h>

using probe::Token;
using probe::TokenType;

namespace
{

bool runProbe(const std::vector<Token>& tokens,
              const MapperGetSubTreeResponse& objects = {},
              const std::vector<std::string>& passed = {})
{
    scan::FoundDevices devices;
    return probe::detail::doProbe(tokens, objects, passed, devices);
}

const Token trueToken{TokenType::boolTrue, ""};
const Token falseToken{TokenType::boolFalse, ""};
const Token opAnd{TokenType::opAnd, ""};
const Token opOr{TokenType::opOr, ""};
const Token opNot{TokenType::opNot, ""};
} // namespace

TEST(DoProbe, EvaluateAndOperator)
{
    EXPECT_TRUE(runProbe({trueToken, opAnd, trueToken}));
    EXPECT_FALSE(runProbe({trueToken, opAnd, falseToken}));
    EXPECT_FALSE(runProbe({falseToken, opAnd, trueToken}));
}

TEST(DoProbe, EvaluateOrOperator)
{
    EXPECT_TRUE(runProbe({falseToken, opOr, trueToken}));
    EXPECT_TRUE(runProbe({trueToken, opOr, falseToken}));
    EXPECT_FALSE(runProbe({falseToken, opOr, falseToken}));
}

TEST(DoProbe, EvaluateNotOperator)
{
    EXPECT_FALSE(runProbe({opNot, trueToken}));
    EXPECT_TRUE(runProbe({opNot, falseToken}));
    EXPECT_TRUE(runProbe({trueToken, opAnd, opNot, falseToken}));
    EXPECT_TRUE(runProbe({opNot, opNot, trueToken}));
}

TEST(DoProbe, EvaluateFoundProbe)
{
    EXPECT_TRUE(runProbe({{TokenType::found, "A"}}, {}, {"A"}));
    EXPECT_FALSE(runProbe({{TokenType::found, "B"}}, {}, {"A"}));
}

TEST(DoProbe, EvaluateDbusProbe)
{
    const DBusInterface properties = {{"PRODUCT_PRODUCT_NAME", std::string("MotherBoard")}};
    const DBusObject interfaces = {{"xyz.openbmc_project.FruDevice", properties}};
    MapperGetSubTreeResponse objects = {{"/xyz/openbmc_project/FruDevice/1", interfaces}};
    scan::FoundDevices devices;

    EXPECT_TRUE(probe::detail::doProbe(
        {{TokenType::dbusProbe,
          "xyz.openbmc_project.FruDevice({'PRODUCT_PRODUCT_NAME': 'MotherBoard'})"}},
        objects, {}, devices));
    ASSERT_EQ(devices.size(), 1U);
    EXPECT_EQ(devices[0].path, "/xyz/openbmc_project/FruDevice/1");

    EXPECT_FALSE(runProbe(
        {{TokenType::dbusProbe,
          "xyz.openbmc_project.FruDevice({'PRODUCT_PRODUCT_NAME': 'Other'})"}},
        objects));
}