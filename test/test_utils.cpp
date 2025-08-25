#include "utils.hpp"

#include <gtest/gtest.h>

TEST(IfindFirstTest, BasicMatch)
{
    auto res = iFindFirst("Hello World", "World");
    EXPECT_EQ(res.first, 6);
    EXPECT_EQ(res.second, 11);
}

TEST(IfindFirstTest, CaseInsensitiveMatch)
{
    auto res = iFindFirst("Hello World", "world");
    EXPECT_EQ(res.first, 6);
    EXPECT_EQ(res.second, 11);
}

TEST(IfindFirstTest, NoMatch)
{
    auto res = iFindFirst("Hello World", "Planet");
    EXPECT_EQ(res.first, std::string_view::npos);
    EXPECT_EQ(res.second, std::string_view::npos);
}

TEST(IfindFirstTest, MatchAtStart)
{
    auto res = iFindFirst("Hello World", "HeLLo");
    EXPECT_EQ(res.first, 0);
    EXPECT_EQ(res.second, 5);
}

TEST(IfindFirstTest, MatchAtEnd)
{
    auto res = iFindFirst("Hello World", "LD");
    EXPECT_EQ(res.first, 9);
    EXPECT_EQ(res.second, 11);
}

TEST(IfindFirstTest, EmptySubstring)
{
    auto res = iFindFirst("Hello", "");
    EXPECT_EQ(res.first, std::string_view::npos);
    EXPECT_EQ(res.second, std::string_view::npos);
}

TEST(IfindFirstTest, EmptyString)
{
    auto res = iFindFirst("", "Hello");
    EXPECT_EQ(res.first, std::string_view::npos);
    EXPECT_EQ(res.second, std::string_view::npos);
}

TEST(SplitTest, NormalSplit)
{
    auto result = split("a,b,c", ',');
    std::vector<std::string> expected = {"a", "b", "c"};
    EXPECT_EQ(result, expected);
}

TEST(SplitTest, ConsecutiveDelimiters)
{
    auto result = split("a,,b", ',');
    std::vector<std::string> expected = {"a", "", "b"};
    EXPECT_EQ(result, expected);
}

TEST(SplitTest, LeadingDelimiter)
{
    auto result = split(",a,b", ',');
    std::vector<std::string> expected = {"", "a", "b"};
    EXPECT_EQ(result, expected);
}

TEST(SplitTest, TrailingDelimiter)
{
    auto result = split("a,b,", ',');
    std::vector<std::string> expected = {"a", "b", ""};
    EXPECT_EQ(result, expected);
}

TEST(SplitTest, NoDelimiter)
{
    auto result = split("abc", ',');
    std::vector<std::string> expected = {"abc"};
    EXPECT_EQ(result, expected);
}

TEST(SplitTest, EmptyString)
{
    auto result = split("", ',');
    std::vector<std::string> expected = {""};
    EXPECT_EQ(result, expected);
}
