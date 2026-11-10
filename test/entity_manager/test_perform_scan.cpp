#include "entity_manager/parsed_config.hpp"
#include "entity_manager/perform_scan.hpp"
#include "entity_manager/topology.hpp"
#include "utils.hpp"

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

using json = nlohmann::json;

static json resolveExposes(EMConfig record)
{
    DBusObject dbusObject = {{"", {}}};
    json configuration = json::object();
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

static void recordScanCompletion(size_t& completions)
{
    ++completions;
}

static void drainPendingBusCalls(sdbusplus::asio::connection& bus)
{
    uint64_t timeout = 0;
    ASSERT_GE(bus.get_timeout(&timeout), 0);
    while (timeout != std::numeric_limits<uint64_t>::max())
    {
        ASSERT_NE(bus.get_io_context().run_one(), 0);
        ASSERT_GE(bus.get_timeout(&timeout), 0);
    }
}

static void addScanConfig(std::vector<ParsedConfig>& configurations,
                          const std::string& name, const std::string& probe)
{
    EMConfig record;
    record.name = name;
    record.type = "Board";
    record.probeStmt = {probe};
    std::optional<ParsedConfig> parsed = ParsedConfig::make(std::move(record));
    ASSERT_TRUE(parsed.has_value());
    configurations.emplace_back(std::move(parsed.value()));
}

TEST(PerformScan, ProcessesConfigurationAfterOneThatDoesNotMatch)
{
    // Keep bus activity separate so the scan's event loop can drain.
    boost::asio::io_context busIo;
    auto bus = std::make_shared<sdbusplus::asio::connection>(
        busIo, sdbusplus::bus::new_default_user());

    boost::asio::io_context io;
    EntityManager em(bus, io, {}, {});
    json missing = json::object();

    // FALSE and TRUE stand for a board that is absent and one that is present;
    // a FruDevice probe would need a D-Bus object.
    std::vector<ParsedConfig> configurations;
    ASSERT_NO_FATAL_FAILURE(
        addScanConfig(configurations, "Yosemite 4 Medusa Board", "FALSE"));
    ASSERT_NO_FATAL_FAILURE(
        addScanConfig(configurations, "Yosemite 4 Management Board", "TRUE"));
    const EMConfig& absent = configurations.at(0).config();
    const EMConfig& present = configurations.at(1).config();

    size_t completions = 0;
    auto scan = std::make_shared<scan::PerformScan>(
        em, missing, configurations, io,
        std::bind_front(recordScanCompletion, std::ref(completions)));

    scan->run();

    EXPECT_FALSE(std::ranges::contains(scan->passedProbes, absent.name));
    EXPECT_TRUE(std::ranges::contains(scan->passedProbes, present.name));
    EXPECT_EQ(em.systemConfiguration, (json{{present.name, present.toJson()}}));
    EXPECT_EQ(completions, 0);

    scan.reset();
    io.run();

    EXPECT_EQ(completions, 1);
    EXPECT_EQ(em.systemConfiguration, (json{{present.name, present.toJson()}}));

    // Complete the initial power-state query while its callback target is
    // alive. sd-bus reports an infinite timeout once no method calls are
    // pending.
    ASSERT_NO_FATAL_FAILURE(drainPendingBusCalls(*bus));
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

// A templated Name is resolved against the device index, and the resolved name
// is registered so that another config's FOUND() on it can match.
TEST(UpdateSystemConfigurationForDevice, RegistersResolvedName)
{
    const std::string probeName = "Nvidia RTX PRO 6000 Blackwell $index";
    EMConfig record;
    record.name = probeName;
    record.type = "Board";
    DBusInterface properties = {{"BUS", uint64_t{10}}};
    const scan::DBusDeviceDescriptor device = {properties, "/fru/blackwell"};
    const MapperGetSubTreeResponse dbusProbeObjects;
    json configuration = json::object();
    Topology topology;
    json missing = json::object();
    std::vector<std::string> passed;
    std::set<json> usedNames;
    std::list<size_t> indexes = {1};
    std::optional<std::string> replaceStr;

    scan::detail::updateSystemConfigurationForDevice(
        record, probeName, device, dbusProbeObjects, configuration, topology,
        missing, passed, usedNames, indexes, replaceStr);

    EXPECT_EQ(passed,
              (std::vector<std::string>{"Nvidia RTX PRO 6000 Blackwell 1"}));
    const std::string recordId =
        scan::detail::getRecordName(properties, probeName);
    EXPECT_EQ(configuration[recordId]["Name"],
              "Nvidia RTX PRO 6000 Blackwell 1");
    EXPECT_TRUE(indexes.empty());
}

// Each found device takes the next index, so every instance registers its own
// resolved name.
TEST(UpdateSystemConfigurationForDevice, RegistersEachInstance)
{
    const std::string probeName = "Nvidia RTX PRO 6000 Blackwell $index";
    EMConfig record;
    record.name = probeName;
    record.type = "Board";
    DBusInterface first = {{"BUS", uint64_t{10}}};
    DBusInterface second = {{"BUS", uint64_t{11}}};
    const MapperGetSubTreeResponse dbusProbeObjects;
    json configuration = json::object();
    Topology topology;
    json missing = json::object();
    std::vector<std::string> passed;
    std::set<json> usedNames;
    std::list<size_t> indexes = {1, 2};
    std::optional<std::string> replaceStr;

    for (const scan::DBusDeviceDescriptor& device :
         {scan::DBusDeviceDescriptor{first, "/fru/blackwell_0"},
          scan::DBusDeviceDescriptor{second, "/fru/blackwell_1"}})
    {
        scan::detail::updateSystemConfigurationForDevice(
            record, probeName, device, dbusProbeObjects, configuration,
            topology, missing, passed, usedNames, indexes, replaceStr);
    }

    EXPECT_EQ(passed,
              (std::vector<std::string>{"Nvidia RTX PRO 6000 Blackwell 1",
                                        "Nvidia RTX PRO 6000 Blackwell 2"}));
}
