// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#include "entity_manager/dbus_interface.hpp"
#include "entity_manager/entity_manager.hpp"
#include "entity_manager/utils.hpp"

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>

#include <gtest/gtest.h>

#ifndef SCHEMA_DIR
#define SCHEMA_DIR "."
#endif

static constexpr const char* kBoardId = "TestBoard";
static constexpr const char* kBoardDbusPath =
    "/xyz/openbmc_project/inventory/system/baseboard/TestBoard";

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

    std::unique_ptr<EntityManager> makeEM()
    {
        auto em = std::make_unique<EntityManager>(
            bus, io, std::vector<std::filesystem::path>{},
            std::filesystem::path{SCHEMA_DIR}, tmpDir);
        em->systemConfiguration[kBoardId] = {
            {"Name", kBoardId},
            {"Type", "Baseboard"},
            {"Exposes", nlohmann::json::array()}};
        return em;
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
                     sysConfig, "/" + std::string{kBoardId}, boardPath,
                     kBoardId);

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

TEST_F(AddObjectTest, AddObjectPersists)
{
    static constexpr const char* kTestService =
        "xyz.openbmc_project.EntityManager.Test.Persist";
    bus->request_name(kTestService);

    auto em = makeEM();

    std::map<sdbusplus::object_path, std::string> newBoards;
    em->postBoardToDBus(kBoardId,
                        em->systemConfiguration[kBoardId]
                            .get_ref<const nlohmann::json::object_t&>(),
                        newBoards);

    const sdbusplus::object_path boardPath{kBoardDbusPath};

    using Params = std::map<std::string, dbus_interface::JsonVariantType>;
    bool done = false;
    bus->async_method_call(
        [&done, &io = io](boost::system::error_code ec) {
            EXPECT_FALSE(ec) << ec.message();
            done = true;
            io.stop();
        },
        kTestService, boardPath, "xyz.openbmc_project.AddObject", "AddObject",
        Params{{"Name", std::string{"Sensor0"}},
               {"Type", std::string{"Temperature"}},
               {"Address", uint64_t{0x40}}});

    io.run_for(std::chrono::seconds(5));
    ASSERT_TRUE(done) << "AddObject D-Bus call did not complete";

    std::ifstream f{tmpDir / "system.json"};
    ASSERT_TRUE(f.is_open()) << "system.json not created by AddObject";
    auto onDisk = nlohmann::json::parse(f);
    const auto& entry = onDisk[kBoardId]["Exposes"][0];
    EXPECT_EQ(entry["Name"], "Sensor0");
    EXPECT_EQ(entry["Type"], "Temperature");
    EXPECT_EQ(entry["Address"], uint64_t{0x40});
}

TEST_F(AddObjectTest, AddObjectDynamicDoesNotPersist)
{
    static constexpr const char* kTestService =
        "xyz.openbmc_project.EntityManager.Test.Dynamic";
    bus->request_name(kTestService);

    auto em = makeEM();

    std::map<sdbusplus::object_path, std::string> newBoards;
    em->postBoardToDBus(kBoardId,
                        em->systemConfiguration[kBoardId]
                            .get_ref<const nlohmann::json::object_t&>(),
                        newBoards);

    const sdbusplus::object_path boardPath{kBoardDbusPath};

    using Params = std::map<std::string, dbus_interface::JsonVariantType>;

    // Seed system.json with a static entry so the file exists before the
    // dynamic call.
    bool done = false;
    bus->async_method_call(
        [&done, &io = io](boost::system::error_code ec) {
            EXPECT_FALSE(ec) << ec.message();
            done = true;
            io.stop();
        },
        kTestService, boardPath, "xyz.openbmc_project.AddObject", "AddObject",
        Params{{"Name", std::string{"StaticSensor"}},
               {"Type", std::string{"Temperature"}}});
    io.run_for(std::chrono::seconds(5));
    ASSERT_TRUE(done) << "AddObject D-Bus call did not complete";

    done = false;
    io.restart();
    bus->async_method_call(
        [&done, &io = io](boost::system::error_code ec) {
            EXPECT_FALSE(ec) << ec.message();
            done = true;
            io.stop();
        },
        kTestService, boardPath, "xyz.openbmc_project.AddObject",
        "AddObjectDynamic",
        Params{{"Name", std::string{"DynSensor"}},
               {"Type", std::string{"Humidity"}}});
    io.run_for(std::chrono::seconds(5));
    ASSERT_TRUE(done) << "AddObjectDynamic D-Bus call did not complete";

    std::ifstream f{tmpDir / "system.json"};
    if (!f.is_open())
    {
        return; // nothing persisted — dynamic entry absent
    }
    auto onDisk = nlohmann::json::parse(f);
    const auto& exposes = onDisk[kBoardId]["Exposes"];
    bool dynFound = std::ranges::any_of(exposes, [](const auto& e) {
        return !e.is_null() && e.value("Name", "") == "DynSensor";
    });
    EXPECT_FALSE(dynFound) << "dynamic entry must not appear in system.json";
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
            sysConfig, "/" + std::string{kBoardId}, boardPath, kBoardId);
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
