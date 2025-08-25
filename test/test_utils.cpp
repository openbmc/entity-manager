#include "utils.hpp"

#include <gtest/gtest.h>

TEST(IfindFirstTest, BasicMatch)
{
    auto res = ifindFirst("Hello World", "World");
    EXPECT_EQ(res.first, 6);
    EXPECT_EQ(res.second, 11);
}

TEST(IfindFirstTest, CaseInsensitiveMatch)
{
    auto res = ifindFirst("Hello World", "world");
    EXPECT_EQ(res.first, 6);
    EXPECT_EQ(res.second, 11);
}

TEST(IfindFirstTest, NoMatch)
{
    auto res = ifindFirst("Hello World", "Planet");
    EXPECT_EQ(res.first, std::string_view::npos);
    EXPECT_EQ(res.second, std::string_view::npos);
}

TEST(IfindFirstTest, MatchAtStart)
{
    auto res = ifindFirst("Hello World", "HeLLo");
    EXPECT_EQ(res.first, 0);
    EXPECT_EQ(res.second, 5);
}

TEST(IfindFirstTest, MatchAtEnd)
{
    auto res = ifindFirst("Hello World", "LD");
    EXPECT_EQ(res.first, 9);
    EXPECT_EQ(res.second, 11);
}

TEST(IfindFirstTest, EmptySubstring)
{
    auto res = ifindFirst("Hello", "");
    EXPECT_EQ(res.first, 0);
    EXPECT_EQ(res.second, 0);
}

TEST(IfindFirstTest, EmptyString)
{
    auto res = ifindFirst("", "Hello");
    EXPECT_EQ(res.first, std::string_view::npos);
    EXPECT_EQ(res.second, std::string_view::npos);
}
