#include "entity_manager/utils.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include "gtest/gtest.h"

using namespace std::string_literals;

TEST(TemplateCharReplace, replaceOneInt)
{
    nlohmann::json j = {{"foo", "$bus"}};
    auto it = j.begin();
    DBusInterface data;
    data["BUS"] = 23;

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = 23;
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, replaceOneStr)
{
    nlohmann::json j = {{"foo", "$TEST"}};
    auto it = j.begin();
    DBusInterface data;
    data["TEST"] = std::string("Test");

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "Test";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, replaceSecondStr)
{
    nlohmann::json j = {{"foo", "the $TEST"}};
    auto it = j.begin();
    DBusInterface data;
    data["TEST"] = std::string("Test");

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "the Test";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, replaceMiddleStr)
{
    nlohmann::json j = {{"foo", "the $TEST worked"}};
    auto it = j.begin();
    DBusInterface data;
    data["TEST"] = std::string("Test");

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "the Test worked";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, replaceLastStr)
{
    nlohmann::json j = {{"foo", "the Test $TEST"}};
    auto it = j.begin();
    DBusInterface data;
    data["TEST"] = 23;

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "the Test 23";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, increment)
{
    nlohmann::json j = {{"foo", "3 plus 1 equals $TEST + 1"}};
    auto it = j.begin();
    DBusInterface data;
    data["TEST"] = 3;

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "3 plus 1 equals 4";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, decrement)
{
    nlohmann::json j = {{"foo", "3 minus 1 equals $TEST - 1 !"}};
    auto it = j.begin();
    DBusInterface data;
    data["TEST"] = 3;

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "3 minus 1 equals 2 !";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, modulus)
{
    nlohmann::json j = {{"foo", "3 mod 2 equals $TEST % 2"}};
    auto it = j.begin();
    DBusInterface data;
    data["TEST"] = 3;

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "3 mod 2 equals 1";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, multiply)
{
    nlohmann::json j = {{"foo", "3 * 2 equals $TEST * 2"}};
    auto it = j.begin();
    DBusInterface data;
    data["TEST"] = 3;

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "3 * 2 equals 6";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, divide)
{
    nlohmann::json j = {{"foo", "4 / 2 equals $TEST / 2"}};
    auto it = j.begin();
    DBusInterface data;
    data["TEST"] = 4;

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "4 / 2 equals 2";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, multiMath)
{
    nlohmann::json j = {{"foo", "4 * 2 % 6 equals $TEST * 2 % 6"}};
    auto it = j.begin();
    DBusInterface data;
    data["TEST"] = 4;

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "4 * 2 % 6 equals 2";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, twoReplacements)
{
    nlohmann::json j = {{"foo", "$FOO $BAR"}};
    auto it = j.begin();
    DBusInterface data;
    data["FOO"] = std::string("foo");
    data["BAR"] = std::string("bar");

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "foo bar";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, twoReplacementsWithMath)
{
    nlohmann::json j = {{"foo", "4 / 2 equals $TEST / 2 $BAR"}};
    auto it = j.begin();
    DBusInterface data;
    data["TEST"] = 4;
    data["BAR"] = std::string("bar");

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "4 / 2 equals 2 bar";
}

TEST(TemplateCharReplace, twoReplacementsWithMath2)
{
    nlohmann::json j = {{"foo", "4 / 2 equals $ADDRESS / 2 $BAR"}};
    auto it = j.begin();
    DBusInterface data;
    data["ADDRESS"] = 4;
    data["BAR"] = std::string("bar");

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "4 / 2 equals 2 bar";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, hexAndWrongCase)
{
    nlohmann::json j = {{"Address", "0x54"},
                        {"Bus", 15},
                        {"Name", "$bus sensor 0"},
                        {"Type", "SomeType"}};

    DBusInterface data;
    data["BUS"] = 15;

    for (auto it = j.begin(); it != j.end(); it++)
    {
        em_utils::templateCharReplace(it, data, 0);
    }
    nlohmann::json expected = {{"Address", 84},
                               {"Bus", 15},
                               {"Name", "15 sensor 0"},
                               {"Type", "SomeType"}};
    EXPECT_EQ(expected, j);
}

TEST(TemplateCharReplace, replaceSecondAsInt)
{
    nlohmann::json j = {{"foo", "twelve is $TEST"}};
    auto it = j.begin();
    DBusInterface data;
    data["test"] = 12;

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = "twelve is 12";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, singleHex)
{
    nlohmann::json j = {{"foo", "0x54"}};
    auto it = j.begin();
    DBusInterface data;

    em_utils::templateCharReplace(it, data, 0);

    nlohmann::json expected = 84;
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, leftOverTemplateVars)
{
    nlohmann::json j = {{"foo", "$EXISTENT_VAR and $NON_EXISTENT_VAR"}};
    auto it = j.begin();

    DBusInterface data;
    data["EXISTENT_VAR"] = std::string("Replaced");

    DBusObject object;
    object["PATH"] = data;

    em_utils::templateCharReplace(it, object, 0);

    nlohmann::json expected = "Replaced and ";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(HandleLeftOverTemplateVars, replaceLeftOverTemplateVar)
{
    nlohmann::json j = {{"foo", "the Test $TEST is $TESTED"}};
    auto it = j.begin();

    em_utils::handleLeftOverTemplateVars(it);

    nlohmann::json expected = "the Test  is ";
    EXPECT_EQ(expected, j["foo"]);
}
