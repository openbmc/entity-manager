// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#include "entity_manager/dbus_interface.hpp"
#include "entity_manager/utils.hpp"

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
