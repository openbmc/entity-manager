// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#include "entity_manager/dbus_interface.hpp"
#include "entity_manager/entity_manager.hpp"

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>

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

TEST_F(AddObjectTest, AddObjectViaDBusCall)
{
    // Unique name avoids conflict with a running entity-manager instance
    static constexpr const char* kTestService =
        "xyz.openbmc_project.EntityManager.Test";
    bus->request_name(kTestService);

    auto em = makeEM();

    // Synchronously registers "xyz.openbmc_project.AddObject" on the board path
    std::map<sdbusplus::object_path, std::string> newBoards;
    em->postBoardToDBus(
        kBoardId,
        em->systemConfiguration[kBoardId]
            .get_ref<const nlohmann::json::object_t&>(),
        newBoards);

    // buildInventorySystemPath("TestBoard", "Baseboard") → "baseboard/TestBoard"
    const sdbusplus::object_path boardPath{
        "/xyz/openbmc_project/inventory/system/baseboard/TestBoard"};

    using Params = std::map<std::string, dbus_interface::JsonVariantType>;
    bool done = false;
    bus->async_method_call(
        [&done, &io = io](boost::system::error_code ec) {
            EXPECT_FALSE(ec) << ec.message();
            done = true;
            io.stop();
        },
        kTestService, boardPath,
        "xyz.openbmc_project.AddObject", "AddObject",
        Params{{"Name", std::string{"Sensor1"}},
               {"Type", std::string{"Temperature"}},
               {"Address", uint64_t{0x41}}});

    io.run_for(std::chrono::seconds(5));
    ASSERT_TRUE(done) << "AddObject D-Bus call did not complete";

    const auto& exposes = em->systemConfiguration[kBoardId]["Exposes"];
    ASSERT_FALSE(exposes.empty());
    EXPECT_EQ(exposes[0].value("Name", ""), "Sensor1");

    std::ifstream f{tmpDir / "system.json"};
    ASSERT_TRUE(f.is_open());
    auto onDisk = nlohmann::json::parse(f);
    EXPECT_EQ(onDisk[kBoardId]["Exposes"][0]["Name"], "Sensor1");
    EXPECT_EQ(onDisk[kBoardId]["Exposes"][0]["Type"], "Temperature");
    EXPECT_EQ(onDisk[kBoardId]["Exposes"][0]["Address"], uint64_t{0x41});
}
