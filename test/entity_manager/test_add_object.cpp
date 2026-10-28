// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#include "entity_manager/dbus_interface.hpp"
#include "entity_manager/utils.hpp"

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <variant>

#include <gtest/gtest.h>

#ifndef SCHEMA_DIR
#define SCHEMA_DIR "."
#endif

static constexpr const char* kBoardId = "TestBoard";

class TestableEMDBusInterface : public dbus_interface::EMDBusInterface
{
  public:
    using dbus_interface::EMDBusInterface::addObject;
    using dbus_interface::EMDBusInterface::EMDBusInterface;
    using dbus_interface::EMDBusInterface::hasConfigObject;
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

TEST_F(AddObjectTest, AddObject)
{
    nlohmann::json sysConfig;
    sysConfig[kBoardId] = {{"Name", kBoardId},
                           {"Type", "Baseboard"},
                           {"Exposes", nlohmann::json::array()}};

    std::string boardName{"TestBoard"};
    const sdbusplus::object_path boardPath =
        em_utils::buildInventorySystemPath(boardName, "Board");

    using Params = std::flat_map<std::string, dbus_interface::JsonVariantType,
                                 std::less<>>;
    iface->addObject(Params{{"Name", std::string{"Sensor1"}},
                            {"Type", std::string{"Temperature"}},
                            {"Address", uint64_t{0x41}}},
                     sysConfig, kBoardId, boardPath, kBoardId);

    const auto& exposes = sysConfig[kBoardId]["Exposes"];
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
    nlohmann::json sysConfig;
    std::string boardName{"TestBoard"};
    const sdbusplus::object_path boardPath =
        em_utils::buildInventorySystemPath(boardName, "Board");

    void setExposes(const nlohmann::json& exposes)
    {
        sysConfig[kBoardId] = {
            {"Name", kBoardId}, {"Type", "Baseboard"}, {"Exposes", exposes}};
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

    const nlohmann::json& exposes()
    {
        return sysConfig[kBoardId]["Exposes"];
    }
};

static nlohmann::json expose(const std::string& name)
{
    return {{"Name", name}, {"Type", "Temperature"}};
}

TEST_F(AddObjectExposesTest, AppendsWhenNoNullSlot)
{
    setExposes({expose("A"), expose("B")});

    add("C");

    ASSERT_EQ(exposes().size(), 3U);
    EXPECT_EQ(exposes()[0]["Name"], "A");
    EXPECT_EQ(exposes()[1]["Name"], "B");
    EXPECT_EQ(exposes()[2]["Name"], "C");
}

TEST_F(AddObjectExposesTest, ReusesFirstNullSlot)
{
    setExposes({expose("A"), nullptr, expose("B"), nullptr});

    add("C");

    ASSERT_EQ(exposes().size(), 4U);
    EXPECT_EQ(exposes()[0]["Name"], "A");
    EXPECT_EQ(exposes()[1]["Name"], "C");
    EXPECT_EQ(exposes()[2]["Name"], "B");
    EXPECT_TRUE(exposes()[3].is_null());

    add("D");

    ASSERT_EQ(exposes().size(), 4U);
    EXPECT_EQ(exposes()[3]["Name"], "D");
}

TEST_F(AddObjectExposesTest, ReusesLeadingNullSlot)
{
    setExposes({nullptr, expose("A")});

    add("B");

    ASSERT_EQ(exposes().size(), 2U);
    EXPECT_EQ(exposes()[0]["Name"], "B");
    EXPECT_EQ(exposes()[1]["Name"], "A");
}

TEST_F(AddObjectExposesTest, DuplicateAfterNullSlotThrows)
{
    setExposes({nullptr, expose("A")});

    EXPECT_THROW(add("A"), std::invalid_argument);
    EXPECT_TRUE(exposes()[0].is_null());
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
    void call(const sdbusplus::object_path& path, const std::string& interface,
              const std::string& method, const Args&... args)
    {
        bool done = false;
        bus->async_method_call(
            [&done](const boost::system::error_code& ec) {
                EXPECT_FALSE(ec) << ec.message();
                done = true;
            },
            bus->get_unique_name(), path.str, interface, method, args...);
        for (int i = 0; !done && i < 100; i++)
        {
            io.run_one_for(std::chrono::milliseconds(100));
        }
        ASSERT_TRUE(done);
        // Delete removes the interface in a posted handler
        io.poll();
    }

    void remove(const sdbusplus::object_path& path, const std::string& type)
    {
        call(path, "xyz.openbmc_project.Configuration." + type, "Delete");
    }
};

TEST_F(ConfigObjectTest, DeleteRemovesConfigObject)
{
    const sdbusplus::object_path path = add("Zone", "Pid.Zone");
    EXPECT_TRUE(iface->hasConfigObject(path));

    remove(path, "Pid.Zone");
    EXPECT_FALSE(iface->hasConfigObject(path));
}

TEST_F(ConfigObjectTest, SharedNameKeepsConfigObjectUntilLastDelete)
{
    const sdbusplus::object_path path = add("Shared", "Pid.Zone");
    add("Shared", "TMP75");
    EXPECT_TRUE(iface->hasConfigObject(path));

    remove(path, "Pid.Zone");
    EXPECT_TRUE(iface->hasConfigObject(path));

    remove(path, "TMP75");
    EXPECT_FALSE(iface->hasConfigObject(path));
}

TEST_F(ConfigObjectTest, RenameKeepsConfigObject)
{
    const sdbusplus::object_path path = add("RenameSource", "Pid.Zone");
    add("RenameSource", "TMP75");

    call(path, "org.freedesktop.DBus.Properties", "Set",
         std::string{"xyz.openbmc_project.Configuration.TMP75"},
         std::string{"Name"}, std::variant<std::string>{"Renamed"});
    remove(path, "Pid.Zone");
    EXPECT_TRUE(iface->hasConfigObject(path));

    remove(path, "TMP75");
    EXPECT_FALSE(iface->hasConfigObject(path));
}
