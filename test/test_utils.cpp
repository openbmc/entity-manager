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

TEST(ToLowerCopyTest, BasicTests)
{
    EXPECT_EQ(toLowerCopy("HelloWorld"), "helloworld");

    EXPECT_EQ(toLowerCopy("HELLOWORLD"), "helloworld");

    EXPECT_EQ(toLowerCopy("helloworld"), "helloworld");

    EXPECT_EQ(toLowerCopy("123ABC!@#"), "123abc!@#");

    EXPECT_EQ(toLowerCopy("!@#$%^&*()_+"), "!@#$%^&*()_+");

    EXPECT_EQ(toLowerCopy(""), "");
}
