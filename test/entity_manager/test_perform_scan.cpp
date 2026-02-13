#include "entity_manager/perform_scan.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
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

// systemConfiguration / missingConfigurations are keyed by a numeric record
// hash (see getRecordName), so these tests use hash-like numeric keys.

// Removes from missingConfigurations any config whose "Name" is in names.
TEST(PruneMissingByName, RemovesConfigsWhoseNameIsInList)
{
    json missing = {{"16888500906263256819", {{"Name", "A"}}},
                    {"3421789056127653902", {{"Name", "B"}}},
                    {"9995127843016654321", {{"Name", "C"}}}};
    std::vector<std::string> names = {"A", "C"};
    scan::detail::pruneMissingByName(missing, names);
    EXPECT_EQ(missing.size(), 1);
    EXPECT_EQ(missing["3421789056127653902"]["Name"], "B");
}

// collectConfiguredNames returns the Name of every systemConfiguration entry
// (the resolved names of already-applied configs that run() seeds into
// passedProbes).
TEST(CollectConfiguredNames, ReturnsAllNames)
{
    json systemConfiguration = {
        {"16888500906263256819", {{"Name", "Nvidia RTX PRO 6000 Blackwell 1"}}},
        {"3421789056127653902", {{"Name", "Nvidia RTX PRO 6000 Blackwell 2"}}}};
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

    // "...Blackwell 1" was resolved from a templated Name and is applied.
    json systemConfiguration = {
        {gpuKey, {{"Name", "Nvidia RTX PRO 6000 Blackwell 1"}}}};

    // At the start of a rescan everything currently present is provisionally
    // "missing" until re-proven this pass.
    json missing = {{gpuKey, {{"Name", "Nvidia RTX PRO 6000 Blackwell 1"}}},
                    {otherKey, {{"Name", "Some Other Board"}}}};

    std::vector<std::string> passedProbes =
        scan::detail::collectConfiguredNames(systemConfiguration);
    scan::detail::pruneMissingByName(missing, passedProbes);

    EXPECT_EQ(missing.size(), 1);
    EXPECT_FALSE(missing.contains(gpuKey));
    EXPECT_TRUE(missing.contains(otherKey));
}
