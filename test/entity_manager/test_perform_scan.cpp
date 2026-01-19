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

static json resolveExposes(EMConfig record)
{
    DBusObject dbusObject = {{"", {}}};
    SystemConfiguration configuration;
    std::optional<std::string> replaceStr;

    scan::detail::applyTemplatesAndExposeActions(
        record.name, record, dbusObject, 1, replaceStr, configuration);
    return record.toJson();
}

static EMConfig bindingConfig()
{
    EMConfig record;
    record.name = "Test Board";
    record.type = "Board";
    record.probeStmt = {"TRUE"};
    record.exposesRecords = {
        {{"Name", "Fan $index"},
         {"Type", "AspeedFan"},
         {"BindConnector", "Connector $index"}},
        {{"Name", "Connector $index"},
         {"Type", "IntelFanConnector"},
         {"Status", "disabled"},
         {"Pwm", 0},
         {"Tachs", json::array({0})},
         {"PwmName", "PWM $index"}}};
    return record;
}

TEST(ExposeActions, BindsTemplatedNameWithinSameConfiguration)
{
    const json resolved = resolveExposes(bindingConfig());
    const auto& fan = resolved.at("Exposes").at(0);

    ASSERT_TRUE(fan.contains("Connector"));
    EXPECT_EQ(fan.at("Connector").at("Name"), "Connector 1");
    EXPECT_EQ(fan.at("Connector").at("PwmName"), "PWM 1");
}

TEST(ExposeActions, ResolvesPropertiesInsertedByBind)
{
    EMConfig record = bindingConfig();
    record.exposesRecords.at(0)["BindConnector"] = "Connector";
    record.exposesRecords.at(1)["Name"] = "Connector";
    record.exposesRecords.at(1)["PwmName"] = "PWM $index $unresolved";
    const json resolved = resolveExposes(record);
    const auto& fan = resolved.at("Exposes").at(0);

    ASSERT_TRUE(fan.contains("Connector"));
    EXPECT_EQ(fan.at("Connector").at("PwmName"), "PWM 1 ");
}

static EMConfig namedConfig(const std::string& name)
{
    EMConfig config;
    config.name = name;
    return config;
}

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

TEST(RestorePersistedConfigurations, RegistersResolvedNameAndPreservesIndex)
{
    const std::string probeName = "Nvidia RTX PRO 6000 Blackwell $index";
    DBusInterface properties = {{"BUS", uint64_t{10}},
                                {"ADDRESS", uint64_t{80}}};
    const std::string recordId =
        scan::detail::getRecordName(properties, probeName);
    SystemConfiguration configuration = {
        {recordId, namedConfig("Nvidia RTX PRO 6000 Blackwell 2")}};
    const json original = configuration.at(recordId).toJson();
    SystemConfiguration cached;
    SystemConfiguration missing = configuration;
    scan::FoundDevices devices = {{properties, "/fru/blackwell"}};
    std::vector<std::string> passed;
    std::set<json> usedNames;
    std::list<size_t> indexes = {1, 2};

    scan::detail::restorePersistedConfigurations(
        devices, probeName, configuration, cached, missing, passed, usedNames,
        indexes);

    EXPECT_EQ(configuration.at(recordId).toJson(), original);
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
    SystemConfiguration configuration = {
        {recordId, namedConfig("Nvidia RTX PRO 6000 Blackwell 1")}};
    SystemConfiguration cached = configuration;
    SystemConfiguration missing = configuration;
    const json original = missing.at(recordId).toJson();
    scan::FoundDevices devices;
    std::vector<std::string> passed;
    std::set<json> usedNames;
    std::list<size_t> indexes = {1};

    scan::detail::restorePersistedConfigurations(
        devices, probeName, configuration, cached, missing, passed, usedNames,
        indexes);

    ASSERT_EQ(missing.size(), 1U);
    EXPECT_EQ(missing.at(recordId).toJson(), original);
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
    SystemConfiguration configuration = {
        {firstId, namedConfig("Nvidia RTX PRO 6000 Blackwell 1")},
        {secondId, namedConfig("Nvidia RTX PRO 6000 Blackwell 2")}};
    SystemConfiguration cached;
    SystemConfiguration missing = configuration;
    scan::FoundDevices devices = {{second, "/fru/blackwell_1"}};
    std::vector<std::string> passed;
    std::set<json> usedNames;
    std::list<size_t> indexes = {1, 2};

    scan::detail::restorePersistedConfigurations(
        devices, probeName, configuration, cached, missing, passed, usedNames,
        indexes);

    ASSERT_EQ(missing.size(), 1U);
    EXPECT_EQ(missing.at(firstId).name, "Nvidia RTX PRO 6000 Blackwell 1");
    EXPECT_EQ(passed,
              (std::vector<std::string>{"Nvidia RTX PRO 6000 Blackwell 2"}));
    EXPECT_EQ(configuration.at(secondId).name,
              "Nvidia RTX PRO 6000 Blackwell 2");
}

TEST(RestorePersistedConfigurations, RestoresCachedMatchedInstance)
{
    const std::string probeName = "Nvidia RTX PRO 6000 Blackwell $index";
    DBusInterface properties = {{"BUS", uint64_t{10}}};
    const std::string recordId =
        scan::detail::getRecordName(properties, probeName);
    SystemConfiguration configuration;
    EMConfig restored = namedConfig("Nvidia RTX PRO 6000 Blackwell 2");
    restored.exposesRecords = {{{"Name", "Sensor"}}};
    SystemConfiguration cached = {{recordId, restored}};
    SystemConfiguration missing;
    scan::FoundDevices devices = {{properties, "/fru/blackwell"}};
    std::vector<std::string> passed;
    std::set<json> usedNames;
    std::list<size_t> indexes = {1, 2};

    scan::detail::restorePersistedConfigurations(
        devices, probeName, configuration, cached, missing, passed, usedNames,
        indexes);

    EXPECT_EQ(configuration.at(recordId).name,
              "Nvidia RTX PRO 6000 Blackwell 2");
    EXPECT_EQ(configuration.at(recordId).toJson().at("Exposes"),
              json::array({{{"Name", "Sensor"}}}));
    EXPECT_EQ(passed,
              (std::vector<std::string>{"Nvidia RTX PRO 6000 Blackwell 2"}));
    EXPECT_TRUE(devices.empty());
    EXPECT_EQ(indexes, (std::list<size_t>{1}));
}
