#include "Utils.hpp"

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <variant>

#include "gtest/gtest.h"

TEST(TemplateCharReplace, replaceOneInt)
{
    nlohmann::json j = {{"foo", "$bus"}};
    auto it = j.begin();
    boost::container::flat_map<std::string, BasicVariantType> data;
    data["BUS"] = 23;

    templateCharReplace(it, data, 0);

    nlohmann::json expected = 23;
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, replaceOneStr)
{
    nlohmann::json j = {{"foo", "$TEST"}};
    auto it = j.begin();
    boost::container::flat_map<std::string, BasicVariantType> data;
    data["TEST"] = std::string("Test");

    templateCharReplace(it, data, 0);

    nlohmann::json expected = "Test";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, replaceSecondStr)
{
    nlohmann::json j = {{"foo", "the $TEST"}};
    auto it = j.begin();
    boost::container::flat_map<std::string, BasicVariantType> data;
    data["TEST"] = std::string("Test");

    templateCharReplace(it, data, 0);

    nlohmann::json expected = "the Test";
    EXPECT_EQ(expected, j["foo"]);
}

/*
TODO This should work

TEST(TemplateCharReplace, replaceMiddleStr)
{
    nlohmann::json j = {{"foo", "the $TEST worked"}};
    auto it = j.begin();
    boost::container::flat_map<std::string, BasicVariantType> data;
    data["TEST"] = std::string("Test");

    templateCharReplace(it, data, 0);

    nlohmann::json expected = "the Test worked";
    EXPECT_EQ(expected, j["foo"]);
}
*/

TEST(TemplateCharReplace, replaceLastStr)
{
    nlohmann::json j = {{"foo", "the Test $TEST"}};
    auto it = j.begin();
    boost::container::flat_map<std::string, BasicVariantType> data;
    data["TEST"] = 23;

    templateCharReplace(it, data, 0);

    nlohmann::json expected = "the Test 23";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, increment)
{
    nlohmann::json j = {{"foo", "3 plus 1 equals $TEST + 1"}};
    auto it = j.begin();
    boost::container::flat_map<std::string, BasicVariantType> data;
    data["TEST"] = 3;

    templateCharReplace(it, data, 0);

    nlohmann::json expected = "3 plus 1 equals 4";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, decrement)
{
    nlohmann::json j = {{"foo", "3 minus 1 equals $TEST - 1 !"}};
    auto it = j.begin();
    boost::container::flat_map<std::string, BasicVariantType> data;
    data["TEST"] = 3;

    templateCharReplace(it, data, 0);

    nlohmann::json expected = "3 minus 1 equals 2 !";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, modulus)
{
    nlohmann::json j = {{"foo", "3 mod 2 equals $TEST % 2"}};
    auto it = j.begin();
    boost::container::flat_map<std::string, BasicVariantType> data;
    data["TEST"] = 3;

    templateCharReplace(it, data, 0);

    nlohmann::json expected = "3 mod 2 equals 1";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, multiply)
{
    nlohmann::json j = {{"foo", "3 * 2 equals $TEST * 2"}};
    auto it = j.begin();
    boost::container::flat_map<std::string, BasicVariantType> data;
    data["TEST"] = 3;

    templateCharReplace(it, data, 0);

    nlohmann::json expected = "3 * 2 equals 6";
    EXPECT_EQ(expected, j["foo"]);
}

TEST(TemplateCharReplace, divide)
{
    nlohmann::json j = {{"foo", "4 / 2 equals $TEST / 2"}};
    auto it = j.begin();
    boost::container::flat_map<std::string, BasicVariantType> data;
    data["TEST"] = 4;

    templateCharReplace(it, data, 0);

    nlohmann::json expected = "4 / 2 equals 2";
    EXPECT_EQ(expected, j["foo"]);
}