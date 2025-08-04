#include "entity_manager/dbus_interface.hpp"

#include "entity_manager/entity_manager.hpp"
#include "entity_manager/utils.hpp"
#include "test_em.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include "gtest/gtest.h"

using namespace std::string_literals;

using DBusPropertiesMap =
    boost::container::flat_map<std::string, std::variant<std::string, int64_t>>;

TEST(DBusInterface, createInterface)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    TestEM em(systemBus, io);

    std::string path = "/xyz/openbmc_project/inventory/mypath";
    std::string interface = "xyz.openbmc_project.Configuration.Example";
    std::string boardNameOrig = "myboard";
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        em.dbus_interface.createInterface(path, interface, boardNameOrig);

    dbus_interface::tryIfaceInitialize(iface);

    auto handler = [&io](boost::system::error_code ec,
                         const DBusPropertiesMap& value) {
        EXPECT_FALSE(ec);
        if (ec)
        {
            lg2::error("{ERROR}", "ERROR", ec.message());
            io.stop();
            return;
        }

        // no properties on this interface
        EXPECT_EQ(value.size(), 0);

        io.stop();
    };

    boost::asio::post(io, [&]() {
        systemBus->async_method_call(handler, em.busName, path,
                                     "org.freedesktop.DBus.Properties",
                                     "GetAll", interface);
    });

    io.run();
}

TEST(DBusInterface, populateInterfaceFromJson)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    TestEM em(systemBus, io);

    nlohmann::json systemConfiguration = "{}";
    nlohmann::json dict = {{"Name", "MyRecord"}, {"X", 3}, {"Type", "Example"}};
    std::string path =
        "/xyz/openbmc_project/inventory/system/board/MyBoard/MyRecord";
    std::string interface = "xyz.openbmc_project.Configuration.Example";
    std::string boardNameOrig = "myboard";

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        em.dbus_interface.createInterface(path, interface, boardNameOrig);

    std::string jsonPointerPath = "xyz";
    em.dbus_interface.populateInterfaceFromJson(systemConfiguration,
                                                jsonPointerPath, iface, dict);

    auto handler = [&io](boost::system::error_code ec,
                         const DBusPropertiesMap& value) {
        EXPECT_FALSE(ec);
        if (ec)
        {
            lg2::error("{ERROR}", "ERROR", ec.message());
            return;
        }

        EXPECT_EQ(value.size(), 3);

        auto name = std::get<std::string>(std::get<1>(*value.find("Name")));
        EXPECT_EQ(name, "MyRecord");

        auto type = std::get<std::string>(std::get<1>(*value.find("Type")));
        EXPECT_EQ(type, "Example");

        io.stop();
    };
    systemBus->async_method_call(handler, em.busName, path,
                                 "org.freedesktop.DBus.Properties", "GetAll",
                                 interface);

    io.run();
}

TEST(DBusInterface, populateIntfPDICompat)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    TestEM em(systemBus, io);

    nlohmann::json systemConfiguration = "{}";
    nlohmann::json dict = {{"Name", "MyRecord"}, {"X", 3}, {"Type", "Example"}};
    std::string path =
        "/xyz/openbmc_project/inventory/system/board/MyBoard/MyRecord";
    std::string interface = "xyz.openbmc_project.Configuration.Example";
    std::string boardNameOrig = "myboard";

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        em.dbus_interface.createInterface(path, interface, boardNameOrig);

    std::string jsonPointerPath = "xyz";

    em.dbus_interface.populateIntfPDICompat(
        systemConfiguration, jsonPointerPath, iface, dict, boardNameOrig);

    auto handler = [&io](boost::system::error_code ec,
                         const DBusPropertiesMap& value) {
        EXPECT_FALSE(ec);
        if (ec)
        {
            lg2::error("{ERROR}", "ERROR", ec.message());
            io.stop();
            return;
        }

        EXPECT_EQ(value.size(), 3);

        auto name = std::get<std::string>(std::get<1>(*value.find("Name")));
        EXPECT_EQ(name, "MyRecord");

        auto type = std::get<std::string>(std::get<1>(*value.find("Type")));
        EXPECT_EQ(type, "Example");

        io.stop();
    };
    systemBus->async_method_call(handler, em.busName, path,
                                 "org.freedesktop.DBus.Properties", "GetAll",
                                 interface);

    io.run();
}

TEST(DBusInterface, populateIntfPDICompatWithNestedObject)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    TestEM em(systemBus, io);

    nlohmann::json systemConfiguration = "{}";
    nlohmann::json dict = {{"Name", "MyRecord"},
                           {"X", {{"Y", 4}, {"Type", "NestedConfig"}}},
                           {"Type", "Example"}};
    std::string path =
        "/xyz/openbmc_project/inventory/system/board/MyBoard/MyRecord";
    std::string interface = "xyz.openbmc_project.Configuration.Example";
    std::string boardNameOrig = "myboard";

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        em.dbus_interface.createInterface(path, interface, boardNameOrig);

    std::string jsonPointerPath = "xyz";

    em.dbus_interface.populateIntfPDICompat(
        systemConfiguration, jsonPointerPath, iface, dict, boardNameOrig);

    auto handler = [&io](boost::system::error_code ec,
                         const DBusPropertiesMap& value) {
        EXPECT_FALSE(ec);
        if (ec)
        {
            lg2::error("{ERROR}", "ERROR", ec.message());
            io.stop();
            return;
        }

        EXPECT_EQ(value.size(), 2);

        ASSERT_TRUE(value.find("Y") != value.end());
        ASSERT_TRUE(value.find("Type") != value.end());

        auto y = std::get<int64_t>(std::get<1>(*value.find("Y")));
        EXPECT_EQ(y, 4);

        auto type = std::get<std::string>(std::get<1>(*value.find("Type")));
        EXPECT_EQ(type, "NestedConfig");

        io.stop();
    };

    std::string nestedPath = path + "/X";
    std::string nestedInterface =
        "xyz.openbmc_project.Configuration.NestedConfig";

    systemBus->async_method_call(handler, em.busName, nestedPath,
                                 "org.freedesktop.DBus.Properties", "GetAll",
                                 nestedInterface);

    io.run();
}

TEST(DBusInterface, populateIntfPDICompatWithNestedArrayOfObjects)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    TestEM em(systemBus, io);

    nlohmann::json systemConfiguration = "{}";

    nlohmann::json::object_t x0 = {{"Y", 5}, {"Type", "NestedConfig"}};
    nlohmann::json::object_t x1 = {{"Y", 6}, {"Type", "NestedConfig"}};

    nlohmann::json::array_t ar = {x0, x1};

    nlohmann::json::object_t obj = {
        {"Name", "MyRecord"}, {"X", ar}, {"Type", "Example"}};

    nlohmann::json dict = obj;

    lg2::debug("json: {JSON}", "JSON", dict);

    std::string path =
        "/xyz/openbmc_project/inventory/system/board/MyBoard/MyRecord";
    std::string interface = "xyz.openbmc_project.Configuration.Example";
    std::string boardNameOrig = "myboard";

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        em.dbus_interface.createInterface(path, interface, boardNameOrig);

    std::string jsonPointerPath = "xyz";

    em.dbus_interface.populateIntfPDICompat(
        systemConfiguration, jsonPointerPath, iface, dict, boardNameOrig);

    auto handler = [&io](boost::system::error_code ec,
                         const DBusPropertiesMap& value) {
        EXPECT_FALSE(ec);
        if (ec)
        {
            lg2::error("{ERROR}", "ERROR", ec.message());
            // io.stop();
            return;
        }

        EXPECT_EQ(value.size(), 2);

        ASSERT_TRUE(value.find("Y") != value.end());
        ASSERT_TRUE(value.find("Type") != value.end());

        auto y = std::get<int64_t>(std::get<1>(*value.find("Y")));
        EXPECT_EQ(y, 5);

        auto type = std::get<std::string>(std::get<1>(*value.find("Type")));
        EXPECT_EQ(type, "NestedConfig");

        io.stop();
    };

    std::string nestedPath = path + "/X/0";
    std::string nestedInterface =
        "xyz.openbmc_project.Configuration.NestedConfig";

    systemBus->async_method_call(handler, em.busName, nestedPath,
                                 "org.freedesktop.DBus.Properties", "GetAll",
                                 nestedInterface);

    io.run();
}
