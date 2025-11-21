#include "dbus_test.hpp"
#include "entity_manager/dbus_interface.hpp"
#include "test_em.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include <gtest/gtest.h>

const std::string configInterface = "xyz.openbmc_project.Configuration.Example";

const std::string objPath =
    "/xyz/openbmc_project/inventory/system/board/MyBoard/MyRecord";

const std::string jsonPointerPath = "xyz";

TEST_F(DBusTest, createInterface)
{
    const std::string path = "/xyz/openbmc_project/inventory/mypath";
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        test.em.dbus_interface.createInterface(path, configInterface,
                                               "myboard");

    dbus_interface::tryIfaceInitialize(iface);

    auto handler = [](const DBusInterface& value) {
        // no properties on this interface
        EXPECT_EQ(value.size(), 0);
    };

    postAssertHandler(handler, path, configInterface);
}

TEST_F(DBusTest, populateInterfaceFromJson_Int)
{
    nlohmann::json dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "X1": 3,
    "X2": -3,
    "Type": "Example"
}
    )");

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        test.em.dbus_interface.createInterface(objPath, configInterface,
                                               "myboard");

    nlohmann::json systemConfiguration = "{}";
    test.em.dbus_interface.populateInterfaceFromJson(
        systemConfiguration, jsonPointerPath, iface, dict);

    auto handler = [](const DBusInterface& value) {
        EXPECT_EQ(value.size(), 4);

        auto name = std::get<std::string>(std::get<1>(*value.find("Name")));
        EXPECT_EQ(name, "MyRecord");

        auto type = std::get<std::string>(std::get<1>(*value.find("Type")));
        EXPECT_EQ(type, "Example");

        EXPECT_EQ(std::get<int64_t>(std::get<1>(*value.find("X1"))), 3);
        EXPECT_EQ(std::get<int64_t>(std::get<1>(*value.find("X2"))), -3);

        EXPECT_EQ(value.find("Y"), value.end());
    };
    postAssertHandler(handler, objPath, configInterface);
}

TEST_F(DBusTest, populateInterfaceFromJson_Double)
{
    nlohmann::json dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "PropertyDouble": 2.0,
    "Type": "Example"
}
    )");

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        test.em.dbus_interface.createInterface(objPath, configInterface,
                                               "myboard");

    nlohmann::json systemConfiguration = "{}";
    test.em.dbus_interface.populateInterfaceFromJson(
        systemConfiguration, jsonPointerPath, iface, dict);

    auto handler = [](const DBusInterface& value) {
        EXPECT_EQ(value.size(), 3);

        EXPECT_EQ(std::get<double>(std::get<1>(*value.find("PropertyDouble"))),
                  2.0);
    };
    postAssertHandler(handler, objPath, configInterface);
}

TEST_F(DBusTest, populateInterfaceFromJson_Bool)
{
    nlohmann::json dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "BoolProp": true,
    "Type": "Example"
}
    )");

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        test.em.dbus_interface.createInterface(objPath, configInterface,
                                               "myboard");

    nlohmann::json systemConfiguration = "{}";
    test.em.dbus_interface.populateInterfaceFromJson(
        systemConfiguration, jsonPointerPath, iface, dict);

    auto handler = [](const DBusInterface& value) {
        EXPECT_EQ(value.size(), 3);

        auto b = std::get<bool>(std::get<1>(*value.find("BoolProp")));
        EXPECT_EQ(b, true);
    };
    postAssertHandler(handler, objPath, configInterface);
}

TEST_F(DBusTest, populateInterfaceFromJson_Arrays_Int)
{
    nlohmann::json dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "ArrayOfInts": [3,2,1],
    "Type": "Example"
}
)");

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        test.em.dbus_interface.createInterface(objPath, configInterface,
                                               "myboard");

    nlohmann::json systemConfiguration = "{}";
    test.em.dbus_interface.populateInterfaceFromJson(
        systemConfiguration, jsonPointerPath, iface, dict);

    auto handler = [](const DBusInterface& value) {
        EXPECT_EQ(value.size(), 3);

        auto x = std::get<std::vector<int64_t>>(
            std::get<1>(*value.find("ArrayOfInts")));
        EXPECT_EQ(x.size(), 3);
        EXPECT_EQ(x[1], 2);
    };
    postAssertHandler(handler, objPath, configInterface);
}

TEST_F(DBusTest, populateInterfaceFromJson_Arrays_Double)
{
    nlohmann::json dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "ArrayOfDouble": [1.3, 1.6, 1.8],
    "Type": "Example"
}
)");

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        test.em.dbus_interface.createInterface(objPath, configInterface,
                                               "myboard");

    nlohmann::json systemConfiguration = "{}";
    test.em.dbus_interface.populateInterfaceFromJson(
        systemConfiguration, jsonPointerPath, iface, dict);

    auto handler = [](const DBusInterface& value) {
        EXPECT_EQ(value.size(), 3);

        EXPECT_TRUE(value.contains("ArrayOfDouble"));

        auto val = *value.find("ArrayOfDouble");

        EXPECT_TRUE(
            std::holds_alternative<std::vector<double>>(std::get<1>(val)));

        // EM quirk: array of bools gets exposed as array of uint64_t
        auto b = std::get<std::vector<double>>(std::get<1>(val));

        EXPECT_EQ(b.size(), 3);
        EXPECT_EQ(b[1], 1.6);
    };
    postAssertHandler(handler, objPath, configInterface);
}

TEST_F(DBusTest, populateInterfaceFromJson_Arrays_String)
{
    nlohmann::json dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "ArrayOfStrings": ["string1", "string2", "string3"],
    "Type": "Example"
}
)");

    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        test.em.dbus_interface.createInterface(objPath, configInterface,
                                               "myboard");

    nlohmann::json systemConfiguration = "{}";
    test.em.dbus_interface.populateInterfaceFromJson(
        systemConfiguration, jsonPointerPath, iface, dict);

    auto handler = [](const DBusInterface& value) {
        EXPECT_EQ(value.size(), 3);

        auto vectorStrings = std::get<std::vector<std::string>>(
            std::get<1>(*value.find("ArrayOfStrings")));
        EXPECT_EQ(vectorStrings.size(), 3);
        EXPECT_EQ(vectorStrings[1], "string2");
    };
    postAssertHandler(handler, objPath, configInterface);
}
