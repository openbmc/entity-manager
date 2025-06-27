#include "entity_manager/utils.hpp"
#include "utils.hpp"

#include <nlohmann/json.hpp>

#include "gtest/gtest.h"

using namespace std::string_literals;

TEST(MatchProbe, stringEqString)
{
    nlohmann::json j = R"("foo")"_json;
    DBusValueVariant v = "foo"s;
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, stringRegexEqString)
{
    nlohmann::json j = R"("foo*")"_json;
    DBusValueVariant v = "foobar"s;
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, stringNeqString)
{
    nlohmann::json j = R"("foobar")"_json;
    DBusValueVariant v = "foo"s;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringRegexError)
{
    nlohmann::json j = R"("foo[")"_json;
    DBusValueVariant v = "foobar"s;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringRegexNotPrefix)
{
    nlohmann::json j = R"("foo(?!bar)...foo")"_json;
    DBusValueVariant v1 = "foobarfoo"s;
    DBusValueVariant v2 = "foofoofoo"s;
    EXPECT_FALSE(matchProbe(j, v1));
    EXPECT_TRUE(matchProbe(j, v2));
}

TEST(MatchProbe, stringZeroNeqFalse)
{
    nlohmann::json j = R"("0")"_json;
    DBusValueVariant v = false;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringOneNeqTrue)
{
    nlohmann::json j = R"("1")"_json;
    DBusValueVariant v = true;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringElevenNeqTrue)
{
    nlohmann::json j = R"("11")"_json;
    DBusValueVariant v = true;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringFalseNeqFalse)
{
    nlohmann::json j = R"("false")"_json;
    DBusValueVariant v = false;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringTrueNeqTrue)
{
    nlohmann::json j = R"("true")"_json;
    DBusValueVariant v = true;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringFalseNeqTrue)
{
    nlohmann::json j = R"("false")"_json;
    DBusValueVariant v = true;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringNeqUint8)
{
    nlohmann::json j = R"("255")"_json;
    DBusValueVariant v = uint8_t(255);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringNeqUint8Overflow)
{
    nlohmann::json j = R"("65535")"_json;
    DBusValueVariant v = uint8_t(255);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringFalseNeqUint8Zero)
{
    nlohmann::json j = R"("false")"_json;
    DBusValueVariant v = uint8_t(0);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringTrueNeqUint8Zero)
{
    nlohmann::json j = R"("true")"_json;
    DBusValueVariant v = uint8_t(1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringNeqUint32)
{
    nlohmann::json j = R"("11")"_json;
    DBusValueVariant v = uint32_t(11);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringNeqInt32)
{
    nlohmann::json j = R"("-11")"_json;
    DBusValueVariant v = int32_t(-11);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringRegexNeqInt32)
{
    nlohmann::json j = R"("1*4")"_json;
    DBusValueVariant v = int32_t(124);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringNeqUint64)
{
    nlohmann::json j = R"("foo")"_json;
    DBusValueVariant v = uint64_t(65535);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringNeqDouble)
{
    nlohmann::json j = R"("123.4")"_json;
    DBusValueVariant v = double(123.4);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringNeqEmpty)
{
    nlohmann::json j = R"("-123.4")"_json;
    DBusValueVariant v;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, stringNeqArray)
{
    nlohmann::json j = R"("-123.4")"_json;
    DBusValueVariant v = std::vector<uint8_t>{1, 2};
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, boolNeqString)
{
    nlohmann::json j = R"(false)"_json;
    DBusValueVariant v = "false"s;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, trueEqTrue)
{
    nlohmann::json j = R"(true)"_json;
    DBusValueVariant v = true;
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, falseEqFalse)
{
    nlohmann::json j = R"(false)"_json;
    DBusValueVariant v = false;
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, trueNeqFalse)
{
    nlohmann::json j = R"(true)"_json;
    DBusValueVariant v = false;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, trueNeqInt32Zero)
{
    nlohmann::json j = R"(true)"_json;
    DBusValueVariant v = int32_t(0);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, trueNeqInt32NegativeOne)
{
    nlohmann::json j = R"(true)"_json;
    DBusValueVariant v = int32_t(-1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, falseNeqUint32One)
{
    nlohmann::json j = R"(false)"_json;
    DBusValueVariant v = uint32_t(1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, falseNeqUint32Zero)
{
    nlohmann::json j = R"(false)"_json;
    DBusValueVariant v = uint32_t(0);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, trueNeqDoubleNegativeOne)
{
    nlohmann::json j = R"(true)"_json;
    DBusValueVariant v = double(-1.1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, trueNeqDoubleOne)
{
    nlohmann::json j = R"(true)"_json;
    DBusValueVariant v = double(1.0);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, falseNeqDoubleOne)
{
    nlohmann::json j = R"(false)"_json;
    DBusValueVariant v = double(1.0);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, falseNeqDoubleZero)
{
    nlohmann::json j = R"(false)"_json;
    DBusValueVariant v = double(0.0);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, falseNeqEmpty)
{
    nlohmann::json j = R"(false)"_json;
    DBusValueVariant v;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, trueNeqEmpty)
{
    nlohmann::json j = R"(true)"_json;
    DBusValueVariant v;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, trueNeqArray)
{
    nlohmann::json j = R"(true)"_json;
    DBusValueVariant v = std::vector<uint8_t>{1, 2};
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, uintNeqString)
{
    nlohmann::json j = R"(11)"_json;
    DBusValueVariant v = "11"s;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, uintNeqTrue)
{
    nlohmann::json j = R"(1)"_json;
    DBusValueVariant v = true;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, uintNeqFalse)
{
    nlohmann::json j = R"(0)"_json;
    DBusValueVariant v = false;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, uintEqUint8)
{
    nlohmann::json j = R"(11)"_json;
    DBusValueVariant v = uint8_t(11);
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, uintNeqUint8)
{
    nlohmann::json j = R"(11)"_json;
    DBusValueVariant v = uint8_t(12);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, uintNeqUint8Overflow)
{
    nlohmann::json j = R"(65535)"_json;
    DBusValueVariant v = uint8_t(255);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, uintEqInt8)
{
    nlohmann::json j = R"(11)"_json;
    DBusValueVariant v = int8_t(11);
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, uintEqDouble)
{
    nlohmann::json j = R"(11)"_json;
    DBusValueVariant v = double(11.0);
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, uintNeqDouble)
{
    nlohmann::json j = R"(11)"_json;
    DBusValueVariant v = double(11.7);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, uintNeqEmpty)
{
    nlohmann::json j = R"(11)"_json;
    DBusValueVariant v;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, uintNeqArray)
{
    nlohmann::json j = R"(11)"_json;
    DBusValueVariant v = std::vector<uint8_t>{11};
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, intNeqString)
{
    nlohmann::json j = R"(-11)"_json;
    DBusValueVariant v = "-11"s;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, intNeqTrue)
{
    nlohmann::json j = R"(-1)"_json;
    DBusValueVariant v = true;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, intNeqUint8)
{
    nlohmann::json j = R"(-11)"_json;
    DBusValueVariant v = uint8_t(11);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, intEqInt8)
{
    nlohmann::json j = R"(-11)"_json;
    DBusValueVariant v = int8_t(-11);
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, intNeqDouble)
{
    nlohmann::json j = R"(-124)"_json;
    DBusValueVariant v = double(-123.0);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, intEqDouble)
{
    nlohmann::json j = R"(-11)"_json;
    DBusValueVariant v = double(-11.0);
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, intNeqDoubleRound)
{
    nlohmann::json j = R"(-11)"_json;
    DBusValueVariant v = double(-11.7);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, intNeqEmpty)
{
    nlohmann::json j = R"(-11)"_json;
    DBusValueVariant v;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, intNeqArray)
{
    nlohmann::json j = R"(-11)"_json;
    DBusValueVariant v = std::vector<uint8_t>{11};
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, doubleNeqString)
{
    nlohmann::json j = R"(0.0)"_json;
    DBusValueVariant v = "0.0"s;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, doubleNeqFalse)
{
    nlohmann::json j = R"(0.0)"_json;
    DBusValueVariant v = false;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, doubleNeqTrue)
{
    nlohmann::json j = R"(1.0)"_json;
    DBusValueVariant v = true;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, doubleEqInt32)
{
    nlohmann::json j = R"(-124.0)"_json;
    DBusValueVariant v = int32_t(-124);
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, doubleNeqInt32)
{
    nlohmann::json j = R"(-124.0)"_json;
    DBusValueVariant v = int32_t(-123);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, doubleRoundNeqInt)
{
    nlohmann::json j = R"(124.7)"_json;
    DBusValueVariant v = int32_t(124);
    EXPECT_FALSE(matchProbe(j, v));
}
TEST(MatchProbe, doubleEqDouble)
{
    nlohmann::json j = R"(-124.2)"_json;
    DBusValueVariant v = double(-124.2);
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, doubleNeqDouble)
{
    nlohmann::json j = R"(-124.3)"_json;
    DBusValueVariant v = double(-124.2);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, doubleNeqEmpty)
{
    nlohmann::json j = R"(-11.0)"_json;
    DBusValueVariant v;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, doubleNeqArray)
{
    nlohmann::json j = R"(-11.2)"_json;
    DBusValueVariant v = std::vector<uint8_t>{11};
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, arrayNeqString)
{
    nlohmann::json j = R"([1, 2])"_json;
    DBusValueVariant v = "hello"s;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, arrayNeqFalse)
{
    nlohmann::json j = R"([1, 2])"_json;
    DBusValueVariant v = false;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, arrayNeqTrue)
{
    nlohmann::json j = R"([1, 2])"_json;
    DBusValueVariant v = true;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, arrayNeqUint8)
{
    nlohmann::json j = R"([1, 2])"_json;
    DBusValueVariant v = uint8_t(1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, arrayNeqInt32)
{
    nlohmann::json j = R"([1, 2])"_json;
    DBusValueVariant v = int32_t(-1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, arrayNeqDouble)
{
    nlohmann::json j = R"([1, 2])"_json;
    DBusValueVariant v = double(1.1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, arrayEqArray)
{
    nlohmann::json j = R"([1, 2])"_json;
    DBusValueVariant v = std::vector<uint8_t>{1, 2};
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, arrayNeqArrayDiffSize1)
{
    nlohmann::json j = R"([1, 2, 3])"_json;
    DBusValueVariant v = std::vector<uint8_t>{1, 2};
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, arrayNeqArrayDiffSize2)
{
    nlohmann::json j = R"([1, 2])"_json;
    DBusValueVariant v = std::vector<uint8_t>{1, 2, 3};
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, emptyArrayEqEmptyArray)
{
    nlohmann::json j = R"([])"_json;
    DBusValueVariant v = std::vector<uint8_t>{};
    EXPECT_TRUE(matchProbe(j, v));
}

TEST(MatchProbe, emptyArrayNeqArray)
{
    nlohmann::json j = R"([])"_json;
    DBusValueVariant v = std::vector<uint8_t>{1};
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, arrayNeqEmptyArray)
{
    nlohmann::json j = R"([1])"_json;
    DBusValueVariant v = std::vector<uint8_t>{};
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, objNeqString)
{
    nlohmann::json j = R"({"foo": "bar"})"_json;
    DBusValueVariant v = "hello"s;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, objNeqFalse)
{
    nlohmann::json j = R"({"foo": "bar"})"_json;
    DBusValueVariant v = false;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, objNeqTrue)
{
    nlohmann::json j = R"({"foo": "bar"})"_json;
    DBusValueVariant v = true;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, objNeqUint8)
{
    nlohmann::json j = R"({"foo": "bar"})"_json;
    DBusValueVariant v = uint8_t(1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, objNeqInt32)
{
    nlohmann::json j = R"({"foo": "bar"})"_json;
    DBusValueVariant v = int32_t(-1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, objNeqDouble)
{
    nlohmann::json j = R"({"foo": "bar"})"_json;
    DBusValueVariant v = double(1.1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, objNeqArray)
{
    nlohmann::json j = R"({"foo": "bar"})"_json;
    DBusValueVariant v = std::vector<uint8_t>{1, 2};
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, nullNeqString)
{
    nlohmann::json j = R"(null)"_json;
    DBusValueVariant v = "hello"s;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, nullNeqFalse)
{
    nlohmann::json j = R"(null)"_json;
    DBusValueVariant v = false;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, nullNeqTrue)
{
    nlohmann::json j = R"(null)"_json;
    DBusValueVariant v = true;
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, nullNeqUint8)
{
    nlohmann::json j = R"(null)"_json;
    DBusValueVariant v = uint8_t(1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, nullNeqInt32)
{
    nlohmann::json j = R"(null)"_json;
    DBusValueVariant v = int32_t(-1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, nullNeqDouble)
{
    nlohmann::json j = R"(null)"_json;
    DBusValueVariant v = double(1.1);
    EXPECT_FALSE(matchProbe(j, v));
}

TEST(MatchProbe, nullNeqArray)
{
    nlohmann::json j = R"(null)"_json;
    DBusValueVariant v = std::vector<uint8_t>{};
    EXPECT_FALSE(matchProbe(j, v));
}
