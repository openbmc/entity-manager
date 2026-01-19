#include "entity_manager/perform_scan.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using json = nlohmann::json;
using probe::Token;
using probe::TokenType;

// parseProbeCommand joins the array statements and lexes them into tokens.
TEST(ParseProbeCommand, ParsesArrayOfStrings)
{
    auto probe = std::vector<std::string>{"FOUND('A')", "FOUND('B')"};
    EXPECT_EQ(
        scan::detail::parseProbeCommand(probe),
        (std::vector<Token>{{TokenType::found, "A"}, {TokenType::found, "B"}}));
}

// A single-string "Probe" field is lexed directly.
TEST(ParseProbeCommand, ParsesSingleString)
{
    auto probe = std::vector<std::string>{"TRUE"};
    EXPECT_EQ(scan::detail::parseProbeCommand(probe),
              (std::vector<Token>{{TokenType::boolTrue, ""}}));
}

// systemConfiguration / missingConfigurations are keyed by a numeric record
// hash (see getRecordName), so these tests use hash-like numeric keys.

// Removes from missingConfigurations any config whose "Name" is in names.
TEST(PruneMissingByName, RemovesConfigsWhoseNameIsInList)
{
    EMConfig c1;
    c1.name = "A";
    EMConfig c2;
    c2.name = "B";
    EMConfig c3;
    c3.name = "C";

    SystemConfiguration missing = {{"16888500906263256819", c1},
                                   {"3421789056127653902", c2},
                                   {"9995127843016654321", c3}};
    std::vector<std::string> names = {"A", "C"};
    scan::detail::pruneMissingByName(missing, names);
    EXPECT_EQ(missing.size(), 1);
    EXPECT_EQ(missing["3421789056127653902"].name, "B");
}

// collectConfiguredNames returns the Name of every systemConfiguration entry
// (the resolved names of already-applied configs that run() seeds into
// passedProbes).
TEST(CollectConfiguredNames, ReturnsAllNames)
{
    EMConfig c1;
    c1.name = "Nvidia RTX PRO 6000 Blackwell 1";
    EMConfig c2;
    c2.name = "Nvidia RTX PRO 6000 Blackwell 2";

    SystemConfiguration systemConfiguration = {{"16888500906263256819", c1},
                                               {"3421789056127653902", c2}};
    std::vector<std::string> names =
        scan::detail::collectConfiguredNames(systemConfiguration);
    EXPECT_EQ(names.size(), 2);
    EXPECT_NE(std::find(names.begin(), names.end(),
                        "Nvidia RTX PRO 6000 Blackwell 1"),
              names.end());
    EXPECT_NE(std::find(names.begin(), names.end(),
                        "Nvidia RTX PRO 6000 Blackwell 2"),
              names.end());
}

// Regression for the templated-config prune bug: on a rescan a config that is
// already applied (its resolved templated Name is in systemConfiguration) must
// survive, while an unrelated missing config stays eligible for pruning. This
// mirrors the seed-then-prune that PerformScan::run() performs.
TEST(SeedAndPrune, RescanKeepsAppliedTemplatedConfig)
{
    const std::string gpuKey = "16888500906263256819";
    const std::string otherKey = "9995127843016654321";

    EMConfig c1;
    c1.name = "Nvidia RTX PRO 6000 Blackwell 1";
    EMConfig c2;
    c2.name = "Some Other Board";

    // "...Blackwell 1" was resolved from a templated Name and is applied.
    SystemConfiguration systemConfiguration = {{gpuKey, c1}};

    // At the start of a rescan everything currently present is provisionally
    // "missing" until re-proven this pass.
    SystemConfiguration missing = {{gpuKey, c1}, {otherKey, c2}};

    std::vector<std::string> passedProbes =
        scan::detail::collectConfiguredNames(systemConfiguration);
    scan::detail::pruneMissingByName(missing, passedProbes);

    EXPECT_EQ(missing.size(), 1);
    EXPECT_FALSE(missing.contains(gpuKey));
    EXPECT_TRUE(missing.contains(otherKey));
}
