// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#include "entity_manager/dbus_interface.hpp"
#include "entity_manager/mctp_target.hpp"
#include "entity_manager/utils.hpp"

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#include <filesystem>
#include <fstream>
#include <memory>

#include <gtest/gtest.h>

#ifndef SCHEMA_DIR
#define SCHEMA_DIR "."
#endif

using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;

static constexpr const char* kBoardId = "TestBoard";

class MCTPTargetTest : public ::testing::Test
{
  protected:
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> bus{
        std::make_shared<sdbusplus::asio::connection>(io)};
    std::filesystem::path tmpDir;
    // Declared in construction order; destroyed in reverse.
    std::unique_ptr<sdbusplus::asio::object_server> objServer;
    std::unique_ptr<ConfigCache> configCache;
    std::unique_ptr<dbus_interface::EMDBusInterface> dbusInterface;
    std::unique_ptr<mctp_target::MCTPTarget> mctpTarget;

    nlohmann::json sysConfig;
    std::string boardName{"TestBoard"};
    const sdbusplus::object_path boardPath =
        em_utils::buildInventorySystemPath(boardName, "Board");

    void SetUp() override
    {
        tmpDir = std::filesystem::temp_directory_path() /
                 ("em_mctp_test_" + std::to_string(getpid()));
        std::filesystem::create_directories(tmpDir);
        objServer = std::make_unique<sdbusplus::asio::object_server>(
            bus, /*skipManager=*/true);
        configCache = std::make_unique<ConfigCache>(tmpDir);
        dbusInterface = std::make_unique<dbus_interface::EMDBusInterface>(
            io, *objServer, std::filesystem::path{SCHEMA_DIR}, *configCache);
        mctpTarget = std::make_unique<mctp_target::MCTPTarget>(*dbusInterface);

        setExposes(nlohmann::json::array());
    }

    void TearDown() override
    {
        // Run posted interface removals before tearing down objServer.
        io.poll();
        std::filesystem::remove_all(tmpDir);
    }

    void setExposes(const nlohmann::json& exposes)
    {
        sysConfig[kBoardId] = {
            {"Name", kBoardId}, {"Type", "Baseboard"}, {"Exposes", exposes}};
    }

    sdbusplus::object_path add(const std::string& name, uint64_t bus,
                               uint8_t address)
    {
        return mctpTarget->addI2CTarget(name, bus, address, sysConfig, kBoardId,
                                        boardPath, kBoardId);
    }

    void remove(const std::string& name)
    {
        mctpTarget->removeI2CTarget(name, sysConfig, kBoardId, boardPath,
                                    kBoardId);
    }

    const nlohmann::json& exposes()
    {
        return sysConfig[kBoardId]["Exposes"];
    }

    nlohmann::json readDisk()
    {
        std::ifstream f{tmpDir / "system.json"};
        EXPECT_TRUE(f.is_open());
        return nlohmann::json::parse(f);
    }
};

TEST_F(MCTPTargetTest, AddCreatesRecord)
{
    const sdbusplus::object_path path = add("MMC", 102, 0x21);

    EXPECT_EQ(path, boardPath / "MMC");

    ASSERT_EQ(exposes().size(), 1U);
    EXPECT_EQ(exposes()[0]["Name"], "MMC");
    EXPECT_EQ(exposes()[0]["Type"], "MCTPI2CTarget");
    EXPECT_EQ(exposes()[0]["Bus"], uint64_t{102});
    EXPECT_EQ(exposes()[0]["Address"], "0x21");

    auto onDisk = readDisk();
    EXPECT_EQ(onDisk[kBoardId]["Exposes"][0]["Name"], "MMC");
    EXPECT_EQ(onDisk[kBoardId]["Exposes"][0]["Type"], "MCTPI2CTarget");
    EXPECT_EQ(onDisk[kBoardId]["Exposes"][0]["Address"], "0x21");
}

TEST_F(MCTPTargetTest, AddRejectsEmptyName)
{
    EXPECT_THROW(add("", 1, 0x10), std::invalid_argument);
    EXPECT_TRUE(exposes().empty());
}

TEST_F(MCTPTargetTest, AddRejectsNon7BitAddress)
{
    EXPECT_THROW(add("Dev", 1, 0x80), std::invalid_argument);
    EXPECT_TRUE(exposes().empty());
}

TEST_F(MCTPTargetTest, AddRejectsDuplicate)
{
    add("Dev", 1, 0x10);
    EXPECT_THROW(add("Dev", 2, 0x11), std::invalid_argument);
    EXPECT_EQ(exposes().size(), 1U);
}

TEST_F(MCTPTargetTest, AddWithoutExposesThrows)
{
    sysConfig[kBoardId] = {{"Name", kBoardId}, {"Type", "Baseboard"}};
    EXPECT_THROW(add("Dev", 1, 0x10), std::invalid_argument);
}

TEST_F(MCTPTargetTest, RemoveClearsRecordAndPersists)
{
    add("Dev", 1, 0x10);

    remove("Dev");

    ASSERT_EQ(exposes().size(), 1U);
    EXPECT_TRUE(exposes()[0].is_null());

    auto onDisk = readDisk();
    EXPECT_TRUE(onDisk[kBoardId]["Exposes"][0].is_null());
}

TEST_F(MCTPTargetTest, RemoveUnknownThrows)
{
    EXPECT_THROW(remove("Missing"), ResourceNotFound);
}

TEST_F(MCTPTargetTest, RemoveIgnoresOtherTypes)
{
    setExposes({{{"Name", "Dev"}, {"Type", "Temperature"}}});

    EXPECT_THROW(remove("Dev"), ResourceNotFound);
    EXPECT_EQ(exposes()[0]["Type"], "Temperature");
}

TEST_F(MCTPTargetTest, AddAfterRemoveReusesSlot)
{
    add("A", 1, 0x10);
    add("B", 1, 0x11);

    remove("A");
    add("C", 2, 0x12);

    ASSERT_EQ(exposes().size(), 2U);
    EXPECT_EQ(exposes()[0]["Name"], "C");
    EXPECT_EQ(exposes()[1]["Name"], "B");
}
