// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#include "entity_manager/config_pointer.hpp"
#include "entity_manager/dbus_interface.hpp"
#include "entity_manager/schema_dbus_types.hpp"
#include "entity_manager/utils.hpp"

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <xyz/openbmc_project/Configuration/Object/common.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#ifndef SCHEMA_DIR
#define SCHEMA_DIR "."
#endif

using ConfigObject =
    sdbusplus::common::xyz::openbmc_project::configuration::Object;

static constexpr const char* kBoardId = "TestBoard";

class TestableEMDBusInterface : public dbus_interface::EMDBusInterface
{
  public:
    using dbus_interface::EMDBusInterface::addObject;
    using dbus_interface::EMDBusInterface::EMDBusInterface;
};

class AddObjectTest : public ::testing::Test
{
  protected:
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> bus{
        std::make_shared<sdbusplus::asio::connection>(io)};
    std::filesystem::path tmpDir;
    // Declared in construction order; destroyed in reverse (iface before
    // objServer/configCache).
    std::unique_ptr<sdbusplus::asio::object_server> objServer;
    std::unique_ptr<ConfigCache> configCache;
    std::unique_ptr<TestableEMDBusInterface> iface;

    void SetUp() override
    {
        tmpDir = std::filesystem::temp_directory_path() /
                 ("em_test_" + std::to_string(getpid()));
        std::filesystem::create_directories(tmpDir);
        objServer = std::make_unique<sdbusplus::asio::object_server>(
            bus, /*skipManager=*/true);
        configCache = std::make_unique<ConfigCache>(tmpDir);
        iface = std::make_unique<TestableEMDBusInterface>(
            io, *objServer, std::filesystem::path{SCHEMA_DIR}, *configCache);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(tmpDir);
    }
};

TEST_F(AddObjectTest, MissingDeviceInterfacesReturnsEmptyVector)
{
    EMConfig device;
    device.name = kBoardId;

    auto& interfaces = iface->getDeviceInterfaces(device);

    EXPECT_TRUE(interfaces.empty());
    EXPECT_EQ(&interfaces, &iface->getDeviceInterfaces(device));
}

TEST_F(AddObjectTest, AddObject)
{
    SystemConfiguration sysConfig;
    sysConfig[kBoardId].name = kBoardId;
    sysConfig[kBoardId].type = "Baseboard";

    std::string boardName{"TestBoard"};
    const sdbusplus::object_path boardPath =
        em_utils::buildInventorySystemPath(boardName, "Board");

    using Params = std::flat_map<std::string, dbus_interface::JsonVariantType,
                                 std::less<>>;
    iface->addObject(Params{{"Name", std::string{"Sensor1"}},
                            {"Type", std::string{"Temperature"}},
                            {"Address", uint64_t{0x41}}},
                     sysConfig, kBoardId, boardPath, kBoardId);

    const auto exposes = sysConfig[kBoardId].toJson().at("Exposes");
    ASSERT_FALSE(exposes.empty());
    EXPECT_EQ(exposes[0].value("Name", ""), "Sensor1");

    std::ifstream f{tmpDir / "system.json"};
    ASSERT_TRUE(f.is_open());
    auto onDisk = nlohmann::json::parse(f);
    EXPECT_EQ(onDisk[kBoardId]["Exposes"][0]["Name"], "Sensor1");
    EXPECT_EQ(onDisk[kBoardId]["Exposes"][0]["Type"], "Temperature");
    EXPECT_EQ(onDisk[kBoardId]["Exposes"][0]["Address"], uint64_t{0x41});
}

class AddObjectExposesTest : public AddObjectTest
{
  protected:
    SystemConfiguration sysConfig;
    std::string boardName{"TestBoard"};
    const sdbusplus::object_path boardPath =
        em_utils::buildInventorySystemPath(boardName, "Board");

    void setExposes(const nlohmann::json& exposes)
    {
        EMConfig& config = sysConfig[kBoardId];
        config.name = kBoardId;
        config.type = "Baseboard";
        config.exposesRecords.clear();
        for (const auto& record : exposes)
        {
            config.exposesRecords.push_back(
                record.get<nlohmann::json::object_t>());
        }
    }

    void add(const std::string& name)
    {
        using Params =
            std::flat_map<std::string, dbus_interface::JsonVariantType,
                          std::less<>>;
        iface->addObject(
            Params{{"Name", name}, {"Type", std::string{"Temperature"}}},
            sysConfig, kBoardId, boardPath, kBoardId);
    }

    nlohmann::json exposes()
    {
        return sysConfig[kBoardId].exposesRecords;
    }
};

static nlohmann::json expose(const std::string& name)
{
    return {{"Name", name}, {"Type", "Temperature"}};
}

static nlohmann::json disabledExpose(const std::string& name)
{
    nlohmann::json record = expose(name);
    record["Status"] = "disabled";
    return record;
}

TEST_F(AddObjectExposesTest, AppendsWhenNoDeletedSlot)
{
    setExposes({expose("A"), expose("B")});

    add("C");

    ASSERT_EQ(exposes().size(), 3U);
    EXPECT_EQ(exposes()[0]["Name"], "A");
    EXPECT_EQ(exposes()[1]["Name"], "B");
    EXPECT_EQ(exposes()[2]["Name"], "C");
}

TEST_F(AddObjectExposesTest, ReusesFirstDeletedSlot)
{
    setExposes({expose("A"), nlohmann::json::object(), expose("B"),
                nlohmann::json::object()});

    add("C");

    ASSERT_EQ(exposes().size(), 4U);
    EXPECT_EQ(exposes()[0]["Name"], "A");
    EXPECT_EQ(exposes()[1]["Name"], "C");
    EXPECT_EQ(exposes()[2]["Name"], "B");
    EXPECT_TRUE(exposes()[3].empty());

    add("D");

    ASSERT_EQ(exposes().size(), 4U);
    EXPECT_EQ(exposes()[3]["Name"], "D");
}

TEST_F(AddObjectExposesTest, ReusesLeadingDeletedSlot)
{
    setExposes({nlohmann::json::object(), expose("A")});

    add("B");

    ASSERT_EQ(exposes().size(), 2U);
    EXPECT_EQ(exposes()[0]["Name"], "B");
    EXPECT_EQ(exposes()[1]["Name"], "A");
}

TEST_F(AddObjectExposesTest, DuplicateAfterDeletedSlotThrows)
{
    setExposes({nlohmann::json::object(), expose("A")});

    EXPECT_THROW(add("A"), std::invalid_argument);
    EXPECT_TRUE(exposes()[0].empty());
    EXPECT_EQ(exposes().size(), 2U);
}

class ConfigObjectTest : public AddObjectExposesTest
{
  protected:
    void SetUp() override
    {
        AddObjectExposesTest::SetUp();
        setExposes(nlohmann::json::array());
    }

    sdbusplus::object_path add(const std::string& name, const std::string& type)
    {
        using Params =
            std::flat_map<std::string, dbus_interface::JsonVariantType,
                          std::less<>>;
        iface->addObject(Params{{"Name", name}, {"Type", type}}, sysConfig,
                         kBoardId, boardPath, kBoardId);
        return boardPath / name;
    }

    template <typename... Args>
    boost::system::error_code tryCall(
        const sdbusplus::object_path& path, const std::string& interface,
        const std::string& method, const Args&... args)
    {
        boost::system::error_code result;
        bool done = false;
        bus->async_method_call(
            [&result, &done](const boost::system::error_code& ec) {
                result = ec;
                done = true;
            },
            bus->get_unique_name(), path, interface, method, args...);
        for (int i = 0; !done && i < 100; i++)
        {
            io.run_one_for(std::chrono::milliseconds(100));
        }
        EXPECT_TRUE(done);
        // Delete removes the interface in a posted handler
        io.poll();
        return result;
    }

    template <typename... Args>
    void call(const sdbusplus::object_path& path, const std::string& interface,
              const std::string& method, const Args&... args)
    {
        boost::system::error_code ec =
            tryCall(path, interface, method, args...);
        EXPECT_FALSE(ec) << ec.message();
    }

    bool exposesConfigObject(const sdbusplus::object_path& path)
    {
        return !tryCall(path, "org.freedesktop.DBus.Properties", "GetAll",
                        std::string{ConfigObject::interface});
    }

    void remove(const sdbusplus::object_path& path, const std::string& type)
    {
        call(path, "xyz.openbmc_project.Configuration." + type, "Delete");
    }
};

TEST_F(ConfigObjectTest, DeleteRemovesConfigObject)
{
    const sdbusplus::object_path path = add("Zone", "Pid.Zone");
    EXPECT_TRUE(exposesConfigObject(path));

    remove(path, "Pid.Zone");
    EXPECT_FALSE(exposesConfigObject(path));
}

TEST_F(ConfigObjectTest, SharedNameKeepsConfigObjectUntilLastDelete)
{
    const sdbusplus::object_path path = add("Shared", "Pid.Zone");
    add("Shared", "TMP75");
    EXPECT_TRUE(exposesConfigObject(path));

    remove(path, "Pid.Zone");
    EXPECT_TRUE(exposesConfigObject(path));

    remove(path, "TMP75");
    EXPECT_FALSE(exposesConfigObject(path));
}

TEST_F(ConfigObjectTest, RenameKeepsConfigObject)
{
    const sdbusplus::object_path path = add("RenameSource", "Pid.Zone");
    add("RenameSource", "TMP75");

    call(path, "org.freedesktop.DBus.Properties", "Set",
         std::string{"xyz.openbmc_project.Configuration.TMP75"},
         std::string{"Name"}, std::variant<std::string>{"Renamed"});
    remove(path, "Pid.Zone");
    EXPECT_TRUE(exposesConfigObject(path));

    remove(path, "TMP75");
    EXPECT_FALSE(exposesConfigObject(path));
}

TEST_F(ConfigObjectTest, RemoveDeviceInterfacesRemovesConfigObject)
{
    const sdbusplus::object_path path = add("Zone", "Pid.Zone");
    EXPECT_TRUE(exposesConfigObject(path));

    iface->removeDeviceInterfaces(sysConfig[kBoardId]);
    EXPECT_FALSE(exposesConfigObject(path));
}

class PublishRecordTest : public AddObjectTest
{
  protected:
    SystemConfiguration sysConfig;

    std::shared_ptr<sdbusplus::asio::dbus_interface> publish(
        const nlohmann::json::object_t& record)
    {
        EMConfig config;
        config.name = kBoardId;
        config.type = "Board";
        config.exposesRecords.push_back(record);

        sysConfig[kBoardId] = config;

        const std::string recordIface = schema_dbus_types::recordInterface(
            record.at("Type").get<std::string>());

        auto published = iface->createInterface(
            sdbusplus::object_path(
                "/xyz/openbmc_project/inventory/system/board/TestBoard/Mcu"),
            recordIface, kBoardId);
        iface->populateInterfaceFromJson(sysConfig, ConfigPointer(kBoardId, 0),
                                         published, record, recordIface);
        return published;
    }
};

TEST_F(PublishRecordTest, PublishesConvertibleRecord)
{
    auto recordIface = publish({{"Name", "Mcu"},
                                {"Type", "MCTPI2CTarget"},
                                {"Address", "0x1d"},
                                {"Bus", 3}});

    ASSERT_TRUE(recordIface->is_initialized());
    // set_property throws if the property has another type, and with
    // changesOnly reports false when the value is already the same.
    EXPECT_FALSE((recordIface->set_property<uint64_t, true>("Address", 29)));
    EXPECT_TRUE(recordIface->set_property("Address", uint64_t{29}));
}

TEST_F(PublishRecordTest, PublishesI3CAddressAsBytes)
{
    auto recordIface = publish(
        {{"Name", "Mcu"},
         {"Type", "MCTPI3CTarget"},
         {"Address", nlohmann::json::array({7, 43, 0, 0, 0, 1})},
         {"Bus", 0}});

    ASSERT_TRUE(recordIface->is_initialized());
    const std::vector<uint8_t> pid{7, 43, 0, 0, 0, 1};
    EXPECT_FALSE((
        recordIface->set_property<std::vector<uint8_t>, true>("Address", pid)));
}

TEST_F(PublishRecordTest, LeavesOutRecordWithUnconvertibleProperty)
{
    auto recordIface = publish({{"Name", "Mcu"},
                                {"Type", "MCTPI2CTarget"},
                                {"Address", "address"},
                                {"Bus", 3}});

    EXPECT_FALSE(recordIface->is_initialized());
}

TEST_F(PublishRecordTest, AddObjectRejectsUnconvertibleRecord)
{
    EMConfig config;
    config.name = kBoardId;
    config.type = "Board";

    sysConfig[kBoardId] = config;

    std::string boardName{kBoardId};
    const sdbusplus::object_path boardPath =
        em_utils::buildInventorySystemPath(boardName, "Board");

    using Params = std::flat_map<std::string, dbus_interface::JsonVariantType,
                                 std::less<>>;
    EXPECT_THROW(iface->addObject(Params{{"Name", std::string{"Mcu"}},
                                         {"Type", std::string{"MCTPI2CTarget"}},
                                         {"Address", std::string{"address"}},
                                         {"Bus", uint64_t{3}}},
                                  sysConfig, kBoardId, boardPath, kBoardId),
                 std::invalid_argument);

    EXPECT_TRUE(sysConfig[kBoardId].exposesRecords.empty());
    EXPECT_FALSE(std::filesystem::exists(tmpDir / "system.json"));
}

// Publishes with schemas written for the test rather than the ones installed.
class SchemaInterfaceTest : public AddObjectTest
{
  protected:
    SystemConfiguration sysConfig;
    std::unique_ptr<TestableEMDBusInterface> typed;

    void SetUp() override
    {
        AddObjectTest::SetUp();
        const std::filesystem::path schemaDir = tmpDir / "schemas";
        std::filesystem::create_directories(schemaDir);
        std::ofstream(schemaDir / "global.json") << R"({
            "$defs": {
                "Board": {
                    "properties": {
                        "xyz.openbmc_project.Inventory.Item.Drive": {
                            "properties": {
                                "Life": {"type": "number", "x-dbus-type": "byte"}
                            }
                        },
                        "Type": {"enum": ["Board"]}
                    }
                },
                "Target": {
                    "properties": {
                        "Address": {"type": "string", "x-dbus-type": "uint64"},
                        "Type": {"const": "BMC"}
                    }
                }
            }
        })";
        typed = std::make_unique<TestableEMDBusInterface>(
            io, *objServer, schemaDir, *configCache);

        EMConfig config;
        config.name = kBoardId;
        config.type = "Board";
        sysConfig[kBoardId] = config;
    }

    std::shared_ptr<sdbusplus::asio::dbus_interface> publish(
        const std::string& name, const nlohmann::json::object_t& dict,
        std::optional<std::string_view> schemaInterface)
    {
        auto published = typed->createInterface(
            sdbusplus::object_path(
                "/xyz/openbmc_project/inventory/system/board/TestBoard"),
            name, kBoardId);
        typed->populateInterfaceFromJson(sysConfig, ConfigPointer(kBoardId, 0),
                                         published, dict, schemaInterface);
        return published;
    }
};

TEST_F(SchemaInterfaceTest, PublishesBoardInterfaceWithItsTypes)
{
    const std::string drive = "xyz.openbmc_project.Inventory.Item.Drive";
    // The Type of the interface is one of its own properties, and is not
    // taken as the Type of a record.
    auto published = publish(drive, {{"Life", 99}, {"Type", "BMC"}}, drive);

    ASSERT_TRUE(published->is_initialized());
    EXPECT_FALSE((published->set_property<uint8_t, true>("Life", 99)));
}

TEST_F(SchemaInterfaceTest, LeavesOutBoardInterfaceThatCannotBeConverted)
{
    const std::string drive = "xyz.openbmc_project.Inventory.Item.Drive";
    auto published = publish(drive, {{"Life", 300}}, drive);

    EXPECT_FALSE(published->is_initialized());
}

TEST_F(SchemaInterfaceTest, PublishesRecordTypesOnAnotherInterface)
{
    // A BMC record is also published on Inventory.Item.Bmc, with the types
    // of its own record.
    auto published =
        publish("xyz.openbmc_project.Inventory.Item.Bmc",
                {{"Name", "Bmc"}, {"Type", "BMC"}, {"Address", "0x10"}},
                schema_dbus_types::recordInterface("BMC"));

    ASSERT_TRUE(published->is_initialized());
    EXPECT_FALSE((published->set_property<uint64_t, true>("Address", 16)));
}

TEST_F(SchemaInterfaceTest, LeavesNestedObjectsUnconverted)
{
    auto published = publish("xyz.openbmc_project.Configuration.BMC.Nested",
                             {{"Address", "0x10"}}, std::nullopt);

    ASSERT_TRUE(published->is_initialized());
    EXPECT_FALSE(
        (published->set_property<std::string, true>("Address", "0x10")));
}

TEST_F(AddObjectExposesTest, PreservesConfiguredDisabledRecords)
{
    const nlohmann::json disabled = disabledExpose("Template");
    setExposes({disabled, nlohmann::json::object(), expose("A")});

    add("B");

    ASSERT_EQ(exposes().size(), 3U);
    EXPECT_EQ(exposes()[0], disabled);
    EXPECT_EQ(exposes()[1], expose("B"));
    EXPECT_EQ(exposes()[2], expose("A"));
}

TEST_F(AddObjectExposesTest, RejectsDuplicateConfiguredDisabledRecord)
{
    const nlohmann::json disabled = disabledExpose("A");
    setExposes(nlohmann::json::array({disabled}));

    EXPECT_THROW(add("A"), std::invalid_argument);

    ASSERT_EQ(exposes().size(), 1U);
    EXPECT_EQ(exposes()[0], disabled);
}

TEST_F(AddObjectExposesTest, ReusesDeletedRecordAndPersistsNullSlot)
{
    setExposes({expose("A"), expose("B")});
    ASSERT_TRUE(ConfigPointer(kBoardId, 0).write(nullptr, sysConfig));
    ASSERT_TRUE(configCache->writeJsonFiles(sysConfig));
    std::ifstream file{tmpDir / "system.json"};
    ASSERT_TRUE(file.is_open());
    const auto cached = nlohmann::json::parse(file);
    EXPECT_TRUE(cached.at(kBoardId).at("Exposes").at(0).is_null());

    add("C");

    ASSERT_EQ(exposes().size(), 2U);
    EXPECT_EQ(exposes()[0], expose("C"));
    EXPECT_EQ(exposes()[1], expose("B"));
}
