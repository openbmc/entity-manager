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
