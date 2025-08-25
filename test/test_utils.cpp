#include "utils.hpp"

#include <gtest/gtest.h>

constexpr std::string_view helloWorld = "Hello World";

TEST(IfindFirstTest, BasicMatch)
{
    auto match = iFindFirst(helloWorld, "World");
    EXPECT_TRUE(match);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.begin()), 6);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.end()), 11);
}

TEST(IfindFirstTest, CaseInsensitiveMatch)
{
    auto match = iFindFirst(helloWorld, "world");
    EXPECT_TRUE(match);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.begin()), 6);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.end()), 11);
}

TEST(IfindFirstTest, NoMatch)
{
    auto match = iFindFirst(helloWorld, "Planet");
    EXPECT_FALSE(match);
}

TEST(IfindFirstTest, MatchAtStart)
{
    auto match = iFindFirst(helloWorld, "HeLLo");
    EXPECT_TRUE(match);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.begin()), 0);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.end()), 5);
}

TEST(IfindFirstTest, MatchAtEnd)
{
    auto match = iFindFirst(helloWorld, "LD");
    EXPECT_TRUE(match);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.begin()), 9);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.end()), 11);
}

TEST(IfindFirstTest, EmptySubstring)
{
    auto match = iFindFirst(helloWorld, "");
    EXPECT_FALSE(match);
}

TEST(IfindFirstTest, EmptyString)
{
    auto match = iFindFirst("", "Hello");
    EXPECT_FALSE(match);
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

TEST(ReplaceAllTest, BasicReplacement)
{
    std::string str = "hello world, world!";
    replaceAll(str, "world", "earth");
    EXPECT_EQ(str, "hello earth, earth!");
}

TEST(ReplaceAllTest, NoMatch)
{
    std::string str = "hello world";
    replaceAll(str, "xxx", "abc");
    EXPECT_EQ(str, "hello world");
}

TEST(ReplaceAllTest, ReplaceWithEmpty)
{
    std::string str = "apple apple";
    replaceAll(str, "apple", "");
    EXPECT_EQ(str, " ");
}

TEST(ReplaceAllTest, ReplaceEmptySearch)
{
    std::string str = "abc";
    replaceAll(str, "", "x");
    EXPECT_EQ(str, "abc");
}

TEST(IReplaceAllTest, CaseInsensitive)
{
    std::string str = "Hello hEllo heLLo";
    iReplaceAll(str, "hello", "hi");
    EXPECT_EQ(str, "hi hi hi");
}

TEST(IReplaceAllTest, MixedContent)
{
    std::string str = "Hello World! WORLD world";
    iReplaceAll(str, "world", "Earth");
    EXPECT_EQ(str, "Hello Earth! Earth Earth");
}

TEST(IReplaceAllTest, NoMatchCaseInsensitive)
{
    std::string str = "Good Morning";
    iReplaceAll(str, "night", "day");
    EXPECT_EQ(str, "Good Morning");
}

TEST(IReplaceAllTest, ReplaceWithEmptyCaseInsensitive)
{
    std::string str = "ABC abc AbC";
    iReplaceAll(str, "abc", "");
    EXPECT_EQ(str, "  ");
}
