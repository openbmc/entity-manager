#include "entity_manager/perform_probe.hpp"

#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
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
    std::optional<probe::Ast> ast = probe::parseProbe(tokens);
    if (!ast.has_value())
    {
        throw std::invalid_argument("did not parse");
    }

    scan::FoundDevices devices;
    return probe::detail::doProbe(ast.value(), objects, passed,
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

std::unique_ptr<probe::Node> leaf(Token token)
{
    return std::make_unique<probe::Node>(
        probe::Node{std::move(token), nullptr, nullptr});
}

std::unique_ptr<probe::Node> join(Token op, std::unique_ptr<probe::Node> left,
                                  std::unique_ptr<probe::Node> right)
{
    return std::make_unique<probe::Node>(
        probe::Node{std::move(op), std::move(left), std::move(right)});
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

TEST(DoProbe, MatchesTemplatedConfigurationNameLiterally)
{
    const std::string name = "Clemente HDD Carrier $bus - 25 % 11";
    const std::vector<Token> tokens = {{TokenType::found, name}};

    EXPECT_TRUE(runProbe(tokens, {}, {name}));
    EXPECT_FALSE(runProbe(tokens, {}, {"Clemente HDD Carrier 0"}));
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

TEST(DoProbe, OperatorsAreLeftAssociativeWithEqualPrecedence)
{
    // AND does not bind tighter than OR: (TRUE OR FALSE) AND FALSE.
    EXPECT_FALSE(runProbe({trueToken, opOr, falseToken, opAnd, falseToken}));

    // The operators are not grouped from the right either:
    // (FALSE AND TRUE) OR TRUE, not FALSE AND (TRUE OR TRUE).
    EXPECT_TRUE(runProbe({falseToken, opAnd, trueToken, opOr, trueToken}));
}

TEST(DoProbe, EvaluatesEveryOperandAndCollectsItsDevices)
{
    // The wBMC variant of the Yosemite 4 Management Board.
    const DBusInterface properties = {
        {"BOARD_PRODUCT_NAME", std::string("Management Board wBMC")},
        {"PRODUCT_PRODUCT_NAME", std::string("Yosemite V4")}};
    const DBusObject interfaces = {
        {"xyz.openbmc_project.FruDevice", properties}};
    MapperGetSubTreeResponse objects = {
        {"/xyz/openbmc_project/FruDevice/1", interfaces}};

    const Token fruProbe =
        dbusProbe("xyz.openbmc_project.FruDevice",
                  {{"BOARD_PRODUCT_NAME", "Management Board wBMC"},
                   {"PRODUCT_PRODUCT_NAME", "Yosemite V4"}});

    // The result is known after TRUE OR, but the D-Bus probe is still
    // evaluated and its device collected.
    scan::FoundDevices orDevices;
    EXPECT_TRUE(runProbe({trueToken, opOr, fruProbe}, objects, {}, &orDevices));
    ASSERT_EQ(orDevices.size(), 1U);
    EXPECT_EQ(orDevices[0].path, "/xyz/openbmc_project/FruDevice/1");

    // The same after FALSE AND, which is false whatever follows.
    scan::FoundDevices andDevices;
    EXPECT_FALSE(
        runProbe({falseToken, opAnd, fruProbe}, objects, {}, &andDevices));
    ASSERT_EQ(andDevices.size(), 1U);
    EXPECT_EQ(andDevices[0].path, "/xyz/openbmc_project/FruDevice/1");
}

TEST(DoProbe, EvaluatesAnAstAsItIsBuilt)
{
    // Read from the left, TRUE OR FALSE AND FALSE is false; this tree groups
    // the AND first, and the evaluation follows the tree.
    std::unique_ptr<probe::Node> andNode =
        join(opAnd, leaf(falseToken), leaf(falseToken));
    probe::Ast ast;
    ast.root = join(opOr, leaf(trueToken), std::move(andNode));

    scan::FoundDevices devices;
    EXPECT_TRUE(probe::detail::doProbe(ast, {}, {}, devices));
}

TEST(DoProbe, EvaluatesTheLeftChildBeforeTheRightChild)
{
    // The two Management Board variants of a Yosemite 4 system.
    const DBusInterface wBmc = {
        {"BOARD_PRODUCT_NAME", std::string("Management Board wBMC")},
        {"PRODUCT_PRODUCT_NAME", std::string("Yosemite V4")}};
    const DBusInterface wnBmc = {
        {"BOARD_PRODUCT_NAME", std::string("Management Board wN-BMC")},
        {"PRODUCT_PRODUCT_NAME", std::string("Yosemite V4")}};
    const DBusObject interfaces1 = {{"xyz.openbmc_project.FruDevice", wBmc}};
    const DBusObject interfaces2 = {{"xyz.openbmc_project.FruDevice", wnBmc}};
    MapperGetSubTreeResponse objects = {
        {"/xyz/openbmc_project/FruDevice/1", interfaces1},
        {"/xyz/openbmc_project/FruDevice/2", interfaces2}};

    std::unique_ptr<probe::Node> left =
        leaf(dbusProbe("xyz.openbmc_project.FruDevice",
                       {{"BOARD_PRODUCT_NAME", "Management Board wN-BMC"},
                        {"PRODUCT_PRODUCT_NAME", "Yosemite V4"}}));
    std::unique_ptr<probe::Node> right =
        leaf(dbusProbe("xyz.openbmc_project.FruDevice",
                       {{"BOARD_PRODUCT_NAME", "Management Board wBMC"},
                        {"PRODUCT_PRODUCT_NAME", "Yosemite V4"}}));
    probe::Ast ast;
    ast.root = join(opOr, std::move(left), std::move(right));

    // Both boards are reported, the board of the left operand first.
    scan::FoundDevices devices;
    EXPECT_TRUE(probe::detail::doProbe(ast, objects, {}, devices));
    ASSERT_EQ(devices.size(), 2U);
    EXPECT_EQ(devices[0].path, "/xyz/openbmc_project/FruDevice/2");
    EXPECT_EQ(devices[1].path, "/xyz/openbmc_project/FruDevice/1");
}

TEST(DoProbe, EmptyAstIsFalse)
{
    scan::FoundDevices devices;
    EXPECT_FALSE(probe::detail::doProbe(probe::Ast{}, {}, {}, devices));
    EXPECT_TRUE(devices.empty());
}
