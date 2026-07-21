#include "entity_manager/perform_scan.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using json = nlohmann::json;

// Removes from missingConfigurations any config whose "Name" is in names.
TEST(PruneMissingByName, RemovesConfigsWhoseNameIsInList)
{
    json missing = {
        {"a", {{"Name", "A"}}}, {"b", {{"Name", "B"}}}, {"c", {{"Name", "C"}}}};
    std::vector<std::string> names = {"A", "C"};
    scan::detail::pruneMissingByName(missing, names);
    EXPECT_EQ(missing.size(), 1);
    EXPECT_EQ(missing["b"]["Name"], "B");
}

// collectConfiguredNames returns the Name of every systemConfiguration entry
// (the resolved names of already-applied configs that run() seeds into
// passedProbes).
TEST(CollectConfiguredNames, ReturnsAllStringNames)
{
    json systemConfiguration = {
        {"key1", {{"Name", "Nvidia RTX PRO 6000 Blackwell 1"}}},
        {"key2", {{"Name", "Nvidia RTX PRO 6000 Blackwell 2"}}}};
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

// Entries without a string Name are skipped rather than throwing.
TEST(CollectConfiguredNames, SkipsEntriesWithoutStringName)
{
    json systemConfiguration = {{"ok", {{"Name", "Good"}}},
                                {"noName", {{"Type", "Foo"}}},
                                {"badType", {{"Name", 42}}}};
    std::vector<std::string> names =
        scan::detail::collectConfiguredNames(systemConfiguration);
    EXPECT_EQ(names.size(), 1);
    EXPECT_EQ(names.front(), "Good");
}

// Regression for the templated-config prune bug: on a rescan a config that is
// already applied (its resolved templated Name is in systemConfiguration) must
// survive, while an unrelated missing config stays eligible for pruning. This
// mirrors the seed-then-prune that PerformScan::run() performs.
TEST(SeedAndPrune, RescanKeepsAppliedTemplatedConfig)
{
    // "...Blackwell 1" was resolved from a templated Name and is applied.
    json systemConfiguration = {
        {"gpu1", {{"Name", "Nvidia RTX PRO 6000 Blackwell 1"}}}};

    // At the start of a rescan everything currently present is provisionally
    // "missing" until re-proven this pass.
    json missing = {{"gpu1", {{"Name", "Nvidia RTX PRO 6000 Blackwell 1"}}},
                    {"other", {{"Name", "Some Other Board"}}}};

    std::vector<std::string> passedProbes =
        scan::detail::collectConfiguredNames(systemConfiguration);
    scan::detail::pruneMissingByName(missing, passedProbes);

    EXPECT_EQ(missing.size(), 1);
    EXPECT_FALSE(missing.contains("gpu1"));
    EXPECT_TRUE(missing.contains("other"));
}

// parseProbeCommand turns an array "Probe" field into a list of statements.
TEST(ParseProbeCommand, ParsesArrayOfStrings)
{
    json probe = json::array({"FOUND('A')", "FOUND('B')"});
    const std::optional<std::vector<std::string>> expected =
        std::vector<std::string>{"FOUND('A')", "FOUND('B')"};
    EXPECT_EQ(scan::detail::parseProbeCommand(probe), expected);
}

// A single-string "Probe" field yields a one-element list.
TEST(ParseProbeCommand, ParsesSingleString)
{
    json probe = "TRUE";
    const std::optional<std::vector<std::string>> expected =
        std::vector<std::string>{"TRUE"};
    EXPECT_EQ(scan::detail::parseProbeCommand(probe), expected);
}

// A non-string statement in the array is rejected (nullopt).
TEST(ParseProbeCommand, ReturnsNulloptOnNonStringElement)
{
    json probe = json::array({"FOUND('A')", 42});
    auto result = scan::detail::parseProbeCommand(probe);
    EXPECT_FALSE(result.has_value());
}
