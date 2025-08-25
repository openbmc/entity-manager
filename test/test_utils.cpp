#include "utils.hpp"

#include <gtest/gtest.h>

TEST(IfindFirstTest, BasicMatch)
{
    auto [firstIndex, lastIndex] = iFindFirst("Hello World", "World");
    EXPECT_EQ(firstIndex, 6);
    EXPECT_EQ(lastIndex, 11);
}

TEST(IfindFirstTest, CaseInsensitiveMatch)
{
    auto [firstIndex, lastIndex] = iFindFirst("Hello World", "world");
    EXPECT_EQ(firstIndex, 6);
    EXPECT_EQ(lastIndex, 11);
}

TEST(IfindFirstTest, NoMatch)
{
    auto [firstIndex, lastIndex] = iFindFirst("Hello World", "Planet");
    EXPECT_EQ(firstIndex, std::string_view::npos);
    EXPECT_EQ(lastIndex, std::string_view::npos);
}

TEST(IfindFirstTest, MatchAtStart)
{
    auto [firstIndex, lastIndex] = iFindFirst("Hello World", "HeLLo");
    EXPECT_EQ(firstIndex, 0);
    EXPECT_EQ(lastIndex, 5);
}

TEST(IfindFirstTest, MatchAtEnd)
{
    auto [firstIndex, lastIndex] = iFindFirst("Hello World", "LD");
    EXPECT_EQ(firstIndex, 9);
    EXPECT_EQ(lastIndex, 11);
}

TEST(IfindFirstTest, EmptySubstring)
{
    auto [firstIndex, lastIndex] = iFindFirst("Hello", "");
    EXPECT_EQ(firstIndex, std::string_view::npos);
    EXPECT_EQ(lastIndex, std::string_view::npos);
}

TEST(IfindFirstTest, EmptyString)
{
    auto [firstIndex, lastIndex] = iFindFirst("", "Hello");
    EXPECT_EQ(firstIndex, std::string_view::npos);
    EXPECT_EQ(lastIndex, std::string_view::npos);
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
