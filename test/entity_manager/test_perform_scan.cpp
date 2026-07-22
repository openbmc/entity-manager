#include "entity_manager/perform_scan.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

#include <gtest/gtest.h>

using json = nlohmann::json;

// parseProbeCommand turns an array "Probe" field into a list of statements.
TEST(ParseProbeCommand, ParsesArrayOfStrings)
{
    json probe = json::array({"FOUND('A')", "FOUND('B')"});
    EXPECT_EQ(scan::detail::parseProbeCommand(probe),
              (std::vector<std::string>{"FOUND('A')", "FOUND('B')"}));
}

// A single-string "Probe" field yields a one-element list.
TEST(ParseProbeCommand, ParsesSingleString)
{
    json probe = "TRUE";
    EXPECT_EQ(scan::detail::parseProbeCommand(probe),
              (std::vector<std::string>{"TRUE"}));
}

// A non-string statement in the array yields an empty vector (the error / not
// a valid probe condition).
TEST(ParseProbeCommand, ReturnsEmptyOnNonStringElement)
{
    json probe = json::array({"FOUND('A')", 42});
    EXPECT_TRUE(scan::detail::parseProbeCommand(probe).empty());
}
