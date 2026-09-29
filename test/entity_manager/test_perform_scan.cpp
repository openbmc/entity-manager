#include "entity_manager/perform_scan.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <list>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using json = nlohmann::json;
using probe::Token;
using probe::TokenType;

// parseProbeCommand joins the array statements and lexes them into tokens.
TEST(ParseProbeCommand, ParsesArrayOfStrings)
{
    json probe = json::array({"FOUND('A')", "FOUND('B')"});
    EXPECT_EQ(
        scan::detail::parseProbeCommand(probe),
        (std::vector<Token>{{TokenType::found, "A"}, {TokenType::found, "B"}}));
}

// A single-string "Probe" field is lexed directly.
TEST(ParseProbeCommand, ParsesSingleString)
{
    json probe = "TRUE";
    EXPECT_EQ(scan::detail::parseProbeCommand(probe),
              (std::vector<Token>{{TokenType::boolTrue, ""}}));
}

// A non-string statement in the array yields an empty vector (the error / not
// a valid probe condition).
TEST(ParseProbeCommand, ReturnsEmptyOnNonStringElement)
{
    json probe = json::array({"FOUND('A')", 42});
    EXPECT_TRUE(scan::detail::parseProbeCommand(probe).empty());
}

TEST(ParseProbeCommand, ParsesDbusPropertiesBeforeEvaluation)
{
    json probe =
        R"(xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Riser\d+', 'ADDRESS': 80}))";
    auto tokens = scan::detail::parseProbeCommand(probe);
    ASSERT_EQ(tokens.size(), 1);
    const auto& token = tokens.at(0);

    ASSERT_TRUE(token.dbus.has_value());

    if (token.dbus.has_value())
    {
        EXPECT_EQ(token.dbus->interface, "xyz.openbmc_project.FruDevice");
        EXPECT_EQ(token.dbus->properties.at("BOARD_PRODUCT_NAME"), "Riser\\d+");
        EXPECT_EQ(token.dbus->properties.at("ADDRESS"), 80);
    }
}

TEST(ParseProbeCommand, RejectsMalformedDbusProperties)
{
    EXPECT_TRUE(scan::detail::parseProbeCommand("xyz.Iface({'A': })").empty());
    EXPECT_TRUE(scan::detail::parseProbeCommand("xyz.Iface([1, 2])").empty());
    EXPECT_TRUE(scan::detail::parseProbeCommand("xyz.Iface(null)").empty());
    EXPECT_TRUE(scan::detail::parseProbeCommand("xyz.Iface({'A': 1}) OR "
                                                "xyz.Iface({'B': })")
                    .empty());
}

TEST(RestorePersistedConfigurations, RegistersResolvedNameAndPreservesIndex)
{
    const std::string probeName = "Nvidia RTX PRO 6000 Blackwell $index";
    DBusInterface properties = {{"BUS", uint64_t{10}},
                                {"ADDRESS", uint64_t{80}}};
    const std::string recordId =
        scan::detail::getRecordName(properties, probeName);
    json configuration = {
        {recordId, {{"Name", "Nvidia RTX PRO 6000 Blackwell 2"}}}};
    const json original = configuration;
    json cached = json::object();
    json missing = configuration;
    scan::FoundDevices devices = {{properties, "/fru/blackwell"}};
    std::vector<std::string> passed;
    std::set<json> usedNames;
    std::list<size_t> indexes = {1, 2};

    scan::detail::restorePersistedConfigurations(
        devices, probeName, configuration, cached, missing, passed, usedNames,
        indexes);

    EXPECT_EQ(configuration, original);
    EXPECT_EQ(passed,
              (std::vector<std::string>{"Nvidia RTX PRO 6000 Blackwell 2"}));
    EXPECT_TRUE(missing.empty());
    EXPECT_TRUE(devices.empty());
    EXPECT_EQ(usedNames, (std::set<json>{"Nvidia RTX PRO 6000 Blackwell 2"}));
    EXPECT_EQ(indexes, (std::list<size_t>{1}));
}

TEST(RestorePersistedConfigurations, MissingDeviceDoesNotRegisterHistoricalName)
{
    const std::string probeName = "Nvidia RTX PRO 6000 Blackwell $index";
    DBusInterface properties = {{"BUS", uint64_t{10}}};
    const std::string recordId =
        scan::detail::getRecordName(properties, probeName);
    json configuration = {
        {recordId, {{"Name", "Nvidia RTX PRO 6000 Blackwell 1"}}}};
    json cached = configuration;
    json missing = configuration;
    const json original = missing;
    scan::FoundDevices devices;
    std::vector<std::string> passed;
    std::set<json> usedNames;
    std::list<size_t> indexes = {1};

    scan::detail::restorePersistedConfigurations(
        devices, probeName, configuration, cached, missing, passed, usedNames,
        indexes);

    EXPECT_EQ(missing, original);
    EXPECT_TRUE(passed.empty());
    EXPECT_TRUE(usedNames.empty());
    EXPECT_EQ(indexes, (std::list<size_t>{1}));
}

TEST(RestorePersistedConfigurations, KeepsUnmatchedInstanceMissing)
{
    const std::string probeName = "Nvidia RTX PRO 6000 Blackwell $index";
    DBusInterface first = {{"ADDRESS", uint64_t{80}}};
    DBusInterface second = {{"ADDRESS", uint64_t{81}}};
    const std::string firstId = scan::detail::getRecordName(first, probeName);
    const std::string secondId = scan::detail::getRecordName(second, probeName);
    json configuration = {
        {firstId, {{"Name", "Nvidia RTX PRO 6000 Blackwell 1"}}},
        {secondId, {{"Name", "Nvidia RTX PRO 6000 Blackwell 2"}}}};
    json cached = json::object();
    json missing = configuration;
    scan::FoundDevices devices = {{second, "/fru/blackwell_1"}};
    std::vector<std::string> passed;
    std::set<json> usedNames;
    std::list<size_t> indexes = {1, 2};

    scan::detail::restorePersistedConfigurations(
        devices, probeName, configuration, cached, missing, passed, usedNames,
        indexes);

    EXPECT_EQ(missing,
              (json{{firstId, {{"Name", "Nvidia RTX PRO 6000 Blackwell 1"}}}}));
    EXPECT_EQ(passed,
              (std::vector<std::string>{"Nvidia RTX PRO 6000 Blackwell 2"}));
    EXPECT_EQ(configuration[secondId]["Name"],
              "Nvidia RTX PRO 6000 Blackwell 2");
}

TEST(RestorePersistedConfigurations, RestoresCachedMatchedInstance)
{
    const std::string probeName = "Nvidia RTX PRO 6000 Blackwell $index";
    DBusInterface properties = {{"BUS", uint64_t{10}}};
    const std::string recordId =
        scan::detail::getRecordName(properties, probeName);
    json configuration = json::object();
    json cached = {
        {recordId,
         {{"Name", "Nvidia RTX PRO 6000 Blackwell 2"},
          {"Exposes", json::array({nullptr, {{"Name", "Sensor"}}})}}}};
    json missing = json::object();
    scan::FoundDevices devices = {{properties, "/fru/blackwell"}};
    std::vector<std::string> passed;
    std::set<json> usedNames;
    std::list<size_t> indexes = {1, 2};

    scan::detail::restorePersistedConfigurations(
        devices, probeName, configuration, cached, missing, passed, usedNames,
        indexes);

    EXPECT_EQ(configuration[recordId]["Name"],
              "Nvidia RTX PRO 6000 Blackwell 2");
    EXPECT_EQ(configuration[recordId]["Exposes"],
              json::array({{{"Name", "Sensor"}}}));
    EXPECT_EQ(passed,
              (std::vector<std::string>{"Nvidia RTX PRO 6000 Blackwell 2"}));
    EXPECT_TRUE(devices.empty());
    EXPECT_EQ(indexes, (std::list<size_t>{1}));
}
