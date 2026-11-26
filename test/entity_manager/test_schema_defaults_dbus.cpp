// SPDX-License-Identifier: Apache-2.0
#include "entity_manager/entity_manager.hpp"

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <flat_map>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

namespace
{
using Json = nlohmann::json;
using Properties = std::flat_map<std::string, dbus_interface::JsonVariantType>;
using DoneHandler = std::function<void(boost::system::error_code)>;
using PropertiesHandler =
    std::function<void(boost::system::error_code, const Properties&)>;
const std::string boardPath =
    "/xyz/openbmc_project/inventory/system/board/Board";
const std::string pid = "xyz.openbmc_project.Configuration.Pid";

class SchemaDefaultsDBusTest : public testing::Test
{
  protected:
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> server;
    std::shared_ptr<sdbusplus::asio::connection> client;
    std::unique_ptr<EntityManager> manager;
    std::filesystem::path directory;
    Properties properties;
    boost::system::error_code lastError;
    bool completed = false;

    void SetUp() override
    {
        auto pattern = (std::filesystem::temp_directory_path() /
                        "entity-manager-defaults-dbus-XXXXXX")
                           .string();
        const char* created = mkdtemp(pattern.data());
        ASSERT_NE(created, nullptr);
        directory = created;
        writeSchema();
        std::filesystem::create_directory(directory / "configs");
        server = std::make_shared<sdbusplus::asio::connection>(
            io, sdbusplus::bus::new_user());
        client = std::make_shared<sdbusplus::asio::connection>(
            io, sdbusplus::bus::new_user());
        manager = std::make_unique<EntityManager>(
            server, io,
            std::vector<std::filesystem::path>{directory / "configs"},
            directory, directory / "cache");
        manager->systemConfiguration = Json::parse(R"({"board": {
            "Name": "Board", "Type": "Board", "Probe": "TRUE",
            "Exposes": [{"Name": "Static", "Type": "Pid", "Nested": {}}]
        }})");
    }

    void TearDown() override
    {
        manager.reset();
        client.reset();
        server.reset();
        if (!directory.empty())
        {
            std::filesystem::remove_all(directory);
        }
    }

    void write(const std::string& name, const std::string& contents)
    {
        std::ofstream stream(directory / name);
        stream << contents;
        ASSERT_TRUE(stream.good());
    }

    void writeSchema()
    {
        write("exposes_record.json", R"({"$ref": "#/$defs/ConfigSchema",
            "$defs": {"ConfigSchema": {"oneOf": [
                {"$ref": "#/$defs/Pid"}
            ]}, "Pid": {"type": "object", "properties": {
                "Type": {"const": "Pid"}, "Name": {"type": "string"},
                "PollRate": {"type": "number", "default": 400},
                "Retries": {"type": "integer", "default": 3, "x-dbus-type": "byte"},
                "Label": {"type": "string", "default": "schema"},
                "Enabled": {"type": "boolean", "default": true},
                "Values": {"type": "array", "items": {"type": "number"}, "default": [1, 2]},
                "Labels": {"type": "array", "items": {"type": "string"}, "default": ["one", "two"]},
                "Nested": {"type": "object", "properties": {
                    "Label": {"type": "string", "default": "nested"}
                }}
            }, "required": ["Name", "Type"]}}})");
    }

    void onProperties(boost::system::error_code error,
                      const Properties& received)
    {
        properties = received;
        onDone(error);
    }

    void onDone(boost::system::error_code error)
    {
        lastError = error;
        completed = true;
        io.stop();
    }

    void wait()
    {
        io.restart();
        io.run_for(std::chrono::seconds(5));
        ASSERT_TRUE(completed) << "D-Bus request timed out";
    }

    void getProperties(const std::string& path, const std::string& interface)
    {
        completed = false;
        properties.clear();
        client->async_method_call(
            PropertiesHandler(
                std::bind_front(&SchemaDefaultsDBusTest::onProperties, this)),
            server->get_unique_name(), path, "org.freedesktop.DBus.Properties",
            "GetAll", interface);
        wait();
    }

    void setProperty(const std::string& path, const std::string& interface,
                     const std::string& name,
                     const dbus_interface::JsonVariantType& value)
    {
        completed = false;
        client->async_method_call(
            DoneHandler(std::bind_front(&SchemaDefaultsDBusTest::onDone, this)),
            server->get_unique_name(), path, "org.freedesktop.DBus.Properties",
            "Set", interface, name, value);
        wait();
    }

    void setRate(const std::string& path, double value)
    {
        setProperty(path, pid, "PollRate", value);
    }

    void addObject()
    {
        completed = false;
        Properties data = {{"Name", std::string("Dynamic")},
                           {"Type", std::string("Pid")}};
        client->async_method_call(
            DoneHandler(std::bind_front(&SchemaDefaultsDBusTest::onDone, this)),
            server->get_unique_name(), boardPath,
            "xyz.openbmc_project.AddObject", "AddObject", data);
        wait();
    }
};

TEST_F(SchemaDefaultsDBusTest, PublishesOnlyScopedProperties)
{
    const auto configured = manager->systemConfiguration;
    manager->postToDbus(configured);
    getProperties(boardPath + "/Static", pid);
    ASSERT_FALSE(lastError) << lastError.message();
    EXPECT_EQ(std::get<double>(properties.at("PollRate")), 400);
    EXPECT_EQ(std::get<uint8_t>(properties.at("Retries")), 3);
    EXPECT_EQ(std::get<std::string>(properties.at("Label")), "schema");
    EXPECT_TRUE(std::get<bool>(properties.at("Enabled")));
    EXPECT_EQ(std::get<std::vector<double>>(properties.at("Values")),
              (std::vector<double>{1, 2}));
    EXPECT_EQ(std::get<std::vector<std::string>>(properties.at("Labels")),
              (std::vector<std::string>{"one", "two"}));
    EXPECT_EQ(manager->systemConfiguration, configured);
    getProperties(boardPath + "/Static", pid + ".Nested");
    ASSERT_FALSE(lastError) << lastError.message();
    EXPECT_TRUE(properties.empty());
}

TEST_F(SchemaDefaultsDBusTest, WritesDefaultedPropertyAtRecordPath)
{
    manager->postToDbus(manager->systemConfiguration);
    setRate(boardPath + "/Static", 123);
    ASSERT_FALSE(lastError) << lastError.message();
    ASSERT_TRUE(manager->systemConfiguration.contains(
        Json::json_pointer("/board/Exposes/0/PollRate")))
        << manager->systemConfiguration.dump();
    EXPECT_EQ(manager->systemConfiguration.at("board").at("Exposes").at(0).at(
                  "PollRate"),
              123);
    EXPECT_FALSE(manager->systemConfiguration.at("board")
                     .at("Exposes")
                     .at(0)
                     .at("Nested")
                     .contains("PollRate"));
    getProperties(boardPath + "/Static", pid);
    ASSERT_FALSE(lastError) << lastError.message();
    EXPECT_EQ(std::get<double>(properties.at("PollRate")), 123);
}

TEST_F(SchemaDefaultsDBusTest, WritesDefaultedArrayAtRecordPath)
{
    manager->postToDbus(manager->systemConfiguration);
    const std::vector<double> values = {3.5, 4.5};
    setProperty(boardPath + "/Static", pid, "Values", values);
    ASSERT_FALSE(lastError) << lastError.message();
    ASSERT_TRUE(manager->systemConfiguration.contains(
        Json::json_pointer("/board/Exposes/0/Values")))
        << manager->systemConfiguration.dump();
    EXPECT_EQ(manager->systemConfiguration.at("board").at("Exposes").at(0).at(
                  "Values"),
              values);
    getProperties(boardPath + "/Static", pid);
    ASSERT_FALSE(lastError) << lastError.message();
    EXPECT_EQ(std::get<std::vector<double>>(properties.at("Values")), values);
}

TEST_F(SchemaDefaultsDBusTest, PublishesPidSlewPosFromRepositorySchema)
{
    manager.reset();
    manager = std::make_unique<EntityManager>(
        server, io, std::vector<std::filesystem::path>{directory / "configs"},
        SCHEMA_DIR, directory / "cache");
    const auto configured = Json::parse(R"({"board": {
        "Name": "Board", "Type": "Board", "Probe": "TRUE",
        "Exposes": [
            {"Name": "Default", "Type": "Pid"},
            {"Name": "Override", "Type": "Pid", "SlewPos": 5}
        ]
    }})");
    manager->systemConfiguration = configured;
    manager->postToDbus(configured);

    getProperties(boardPath + "/Default", pid);
    ASSERT_FALSE(lastError) << lastError.message();
    EXPECT_EQ(std::get<double>(properties.at("SlewPos")), 0);

    getProperties(boardPath + "/Override", pid);
    ASSERT_FALSE(lastError) << lastError.message();
    EXPECT_EQ(std::get<double>(properties.at("SlewPos")), 5);
    EXPECT_EQ(manager->systemConfiguration, configured);
}

TEST_F(SchemaDefaultsDBusTest, AddObjectUsesSameDefaultsAndSupportsOverrides)
{
    manager->postToDbus(manager->systemConfiguration);
    addObject();
    ASSERT_FALSE(lastError) << lastError.message();
    getProperties(boardPath + "/Static", pid);
    ASSERT_FALSE(lastError) << lastError.message();
    const auto staticProperties = properties;
    getProperties(boardPath + "/Dynamic", pid);
    ASSERT_FALSE(lastError) << lastError.message();
    EXPECT_EQ(properties.at("PollRate"), staticProperties.at("PollRate"));
    EXPECT_EQ(properties.at("Retries"), staticProperties.at("Retries"));
    EXPECT_EQ(properties.at("Label"), staticProperties.at("Label"));
    EXPECT_FALSE(
        manager->systemConfiguration.at("board").at("Exposes").at(1).contains(
            "PollRate"));
    setRate(boardPath + "/Dynamic", 789);
    ASSERT_FALSE(lastError) << lastError.message();
    ASSERT_TRUE(manager->systemConfiguration.contains(
        Json::json_pointer("/board/Exposes/1/PollRate")))
        << manager->systemConfiguration.dump();
    EXPECT_EQ(manager->systemConfiguration.at("board").at("Exposes").at(1).at(
                  "PollRate"),
              789);
}
} // namespace
