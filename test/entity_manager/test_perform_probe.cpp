#include "entity_manager/perform_probe.hpp"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using probe::Token;
using probe::TokenType;

namespace
{

bool runProbe(const std::vector<Token>& tokens,
              const MapperGetSubTreeResponse& objects = {},
              const std::vector<std::string>& passed = {},
              scan::FoundDevices* foundDevs = nullptr)
{
    scan::FoundDevices devices;
    return probe::detail::doProbe(tokens, objects, passed,
                                  foundDevs != nullptr ? *foundDevs : devices);
}

const Token trueToken{TokenType::boolTrue, ""};
const Token falseToken{TokenType::boolFalse, ""};
const Token opAnd{TokenType::opAnd, ""};
const Token opOr{TokenType::opOr, ""};

Token dbusProbe(const std::string& interface,
                std::map<std::string, nlohmann::json> properties)
{
    return {TokenType::dbusProbe, "",
            probe::DbusProbe{interface, std::move(properties)}};
}
} // namespace

TEST(DoProbe, EvaluateAndOperator)
{
    EXPECT_TRUE(runProbe({trueToken, opAnd, trueToken}));
    EXPECT_FALSE(runProbe({trueToken, opAnd, falseToken}));
    EXPECT_FALSE(runProbe({falseToken, opAnd, trueToken}));
    EXPECT_FALSE(runProbe({falseToken, opAnd, falseToken}));
}

TEST(DoProbe, EvaluateOrOperator)
{
    EXPECT_TRUE(runProbe({trueToken, opOr, trueToken}));
    EXPECT_TRUE(runProbe({falseToken, opOr, trueToken}));
    EXPECT_TRUE(runProbe({trueToken, opOr, falseToken}));
    EXPECT_FALSE(runProbe({falseToken, opOr, falseToken}));
}

TEST(DoProbe, EvaluateFoundProbe)
{
    EXPECT_TRUE(runProbe({{TokenType::found, "A"}}, {}, {"A"}));
    EXPECT_FALSE(runProbe({{TokenType::found, "B"}}, {}, {"A"}));
}

TEST(DoProbe, EvaluateDbusProbe)
{
    const DBusInterface properties = {
        {"PRODUCT_PRODUCT_NAME", std::string("MotherBoard")}};
    const DBusObject interfaces = {
        {"xyz.openbmc_project.FruDevice", properties}};
    MapperGetSubTreeResponse objects = {
        {"/xyz/openbmc_project/FruDevice/1", interfaces}};

    scan::FoundDevices devices;
    EXPECT_TRUE(runProbe({dbusProbe("xyz.openbmc_project.FruDevice",
                                    {{"PRODUCT_PRODUCT_NAME", "MotherBoard"}})},
                         objects, {}, &devices));
    ASSERT_EQ(devices.size(), 1U);
    EXPECT_EQ(devices[0].path, "/xyz/openbmc_project/FruDevice/1");

    EXPECT_FALSE(runProbe({dbusProbe("xyz.openbmc_project.FruDevice",
                                     {{"PRODUCT_PRODUCT_NAME", "Other"}})},
                          objects));
}

TEST(DoProbe, MatchOneRetainsSingleDevice)
{
    const DBusInterface properties1 = {
        {"PRODUCT_PRODUCT_NAME", std::string("Board_1")}};
    const DBusInterface properties2 = {
        {"PRODUCT_PRODUCT_NAME", std::string("Board_2")}};
    const DBusObject interfaces1 = {
        {"xyz.openbmc_project.FruDevice", properties1}};
    const DBusObject interfaces2 = {
        {"xyz.openbmc_project.FruDevice", properties2}};
    MapperGetSubTreeResponse objects = {
        {"/xyz/openbmc_project/FruDevice/1", interfaces1},
        {"/xyz/openbmc_project/FruDevice/2", interfaces2}};

    const Token fruProbe = dbusProbe("xyz.openbmc_project.FruDevice",
                                     {{"PRODUCT_PRODUCT_NAME", "Board"}});

    // Without MATCH_ONE, all matching objects are reported.
    scan::FoundDevices devices;
    EXPECT_TRUE(runProbe({fruProbe}, objects, {}, &devices));
    EXPECT_EQ(devices.size(), 2U);

    // MATCH_ONE retains only the last matching object.
    scan::FoundDevices matchOneDevices;
    EXPECT_TRUE(runProbe({fruProbe, {TokenType::matchOne, ""}}, objects, {},
                         &matchOneDevices));
    ASSERT_EQ(matchOneDevices.size(), 1U);
    EXPECT_EQ(matchOneDevices[0].path, "/xyz/openbmc_project/FruDevice/2");
}
