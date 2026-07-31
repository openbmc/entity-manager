// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#include "entity_manager/dbus_interface.hpp"
#include "entity_manager/entity_manager.hpp"

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <filesystem>
#include <fstream>
#include <memory>

#include <gtest/gtest.h>

#ifndef SCHEMA_DIR
#define SCHEMA_DIR "."
#endif

static constexpr const char* kBoardId = "TestBoard";
static constexpr const char* kBoardJsonPtr = "/TestBoard";
static constexpr const char* kBoardDbusPath =
    "/xyz/openbmc_project/inventory/system/board/TestBoard";

class AddObjectTest : public ::testing::Test
{
  protected:
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> bus{
        std::make_shared<sdbusplus::asio::connection>(io)};
    std::filesystem::path tmpDir;

    void SetUp() override
    {
        tmpDir = std::filesystem::temp_directory_path() /
                 ("em_test_" + std::to_string(getpid()));
        std::filesystem::create_directories(tmpDir);
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

TEST_F(AddObjectTest, AddObjectPersistsDynamicDoesNot)
{
    auto em = makeEM();

    // Non-dynamic: system.json must be created with all supplied fields.
    nlohmann::json data = {
        {"Name", "Sensor0"}, {"Type", "Temperature"}, {"Address", 0x40}};
    em->dbus_interface.addObjectJson(
        data, em->systemConfiguration, kBoardJsonPtr,
        sdbusplus::object_path{kBoardDbusPath}, kBoardId, /*isDynamic=*/false);
    io.poll();

    {
        std::ifstream f{tmpDir / "system.json"};
        ASSERT_TRUE(f.is_open()) << "system.json not created by AddObject";
        auto onDisk = nlohmann::json::parse(f);
        const auto& entry = onDisk[kBoardId]["Exposes"][0];
        EXPECT_EQ(entry["Name"], "Sensor0");
        EXPECT_EQ(entry["Type"], "Temperature");
        EXPECT_EQ(entry["Address"], 0x40);
    }

    // Dynamic: system.json must remain unchanged — the new entry is absent.
    nlohmann::json dynData = {{"Name", "DynSensor"}, {"Type", "Humidity"}};
    em->dbus_interface.addObjectJson(
        dynData, em->systemConfiguration, kBoardJsonPtr,
        sdbusplus::object_path{kBoardDbusPath}, kBoardId, /*isDynamic=*/true);
    io.poll();

    {
        std::ifstream f{tmpDir / "system.json"};
        ASSERT_TRUE(f.is_open()) << "system.json must survive AddObjectDynamic";
        auto onDisk = nlohmann::json::parse(f);
        const auto& exposes = onDisk[kBoardId]["Exposes"];
        EXPECT_EQ(exposes[0]["Name"], "Sensor0");
        bool dynFound = std::ranges::any_of(exposes, [](const auto& e) {
            return !e.is_null() && e.value("Name", "") == "DynSensor";
        });
        EXPECT_FALSE(dynFound)
            << "dynamic entry must not appear in system.json";
    }
}
