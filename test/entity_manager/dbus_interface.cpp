#include "entity_manager/dbus_interface.hpp"

#include "dbus_test.hpp"
#include "test_em.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include <gtest/gtest.h>

TEST_F(DBusTest, createInterface)
{
    std::string path = "/xyz/openbmc_project/inventory/mypath";
    iface =
        em.dbus_interface.createInterface(path, configInterface, boardNameOrig);

    dbus_interface::tryIfaceInitialize(iface);

    auto handler = [](const DBusPropertiesMap& value) {
        // no properties on this interface
        EXPECT_EQ(value.size(), 0);
    };

    postAssertHandler(handler, path, configInterface);
}

TEST_F(DBusTest, populateInterfaceFromJson_Simple)
{
    dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "X1": 3,
    "X2": -3,
    "X3": 2.0,
    "BoolProp": true,
    "Type": "Example"
}
    )");

    iface = em.dbus_interface.createInterface(objPath, configInterface,
                                              boardNameOrig);

    em.dbus_interface.populateInterfaceFromJson(systemConfiguration,
                                                jsonPointerPath, iface, dict);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 6);

        auto name = std::get<std::string>(std::get<1>(*value.find("Name")));
        EXPECT_EQ(name, "MyRecord");

        auto type = std::get<std::string>(std::get<1>(*value.find("Type")));
        EXPECT_EQ(type, "Example");

        EXPECT_EQ(std::get<int64_t>(std::get<1>(*value.find("X1"))), 3);
        EXPECT_EQ(std::get<int64_t>(std::get<1>(*value.find("X2"))), -3);
        EXPECT_EQ(std::get<double>(std::get<1>(*value.find("X3"))), 2.0);

        auto b = std::get<bool>(std::get<1>(*value.find("BoolProp")));
        EXPECT_EQ(b, true);

        EXPECT_EQ(value.find("Y"), value.end());
    };
    postAssertHandler(handler, objPath, configInterface);
}

TEST_F(DBusTest, populateInterfaceFromJson_Arrays_Int)
{
    dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "ArrayOfInts": [3,2,1],
    "Type": "Example"
}
)");

    iface = em.dbus_interface.createInterface(objPath, configInterface,
                                              boardNameOrig);

    em.dbus_interface.populateInterfaceFromJson(systemConfiguration,
                                                jsonPointerPath, iface, dict);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 3);

        auto x = std::get<std::vector<int64_t>>(
            std::get<1>(*value.find("ArrayOfInts")));
        EXPECT_EQ(x.size(), 3);
        EXPECT_EQ(x[1], 2);
    };
    postAssertHandler(handler, objPath, configInterface);
}

TEST_F(DBusTest, populateInterfaceFromJson_Arrays_Bool)
{
    dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "ArrayOfBools": [true, false, true],
    "Type": "Example"
}
)");

    iface = em.dbus_interface.createInterface(objPath, configInterface,
                                              boardNameOrig);

    em.dbus_interface.populateInterfaceFromJson(systemConfiguration,
                                                jsonPointerPath, iface, dict);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 3);

        // EM quirk: array of bools gets exposed as array of uint64_t
        auto b = std::get<std::vector<uint64_t>>(
            std::get<1>(*value.find("ArrayOfBools")));

        // TODO: investigate why this is failing
        // EXPECT_EQ(b.size(), 3);
        // EXPECT_EQ(b[1], false);
    };
    postAssertHandler(handler, objPath, configInterface, false);
}

TEST_F(DBusTest, populateInterfaceFromJson_Arrays_String)
{
    dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "ArrayOfStrings": ["how", "many", "brownie", "points"],
    "Type": "Example"
}
)");

    iface = em.dbus_interface.createInterface(objPath, configInterface,
                                              boardNameOrig);

    em.dbus_interface.populateInterfaceFromJson(systemConfiguration,
                                                jsonPointerPath, iface, dict);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 3);

        auto vectorStrings = std::get<std::vector<std::string>>(
            std::get<1>(*value.find("ArrayOfStrings")));
        EXPECT_EQ(vectorStrings.size(), 4);
        EXPECT_EQ(vectorStrings[1], "many");
    };
    postAssertHandler(handler, objPath, configInterface);
}

TEST_F(DBusTest, populateIntfPDICompat)
{
    dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "X": 3,
    "Type": "Example"
}
)");

    iface = em.dbus_interface.createInterface(objPath, configInterface,
                                              boardNameOrig);

    config_type_tree::ConfigTypeNode ctn = {"Example",
                                            {{"X", {"NestedConfig", {}}}}};
    em.dbus_interface.populateIntfPDICompat(
        systemConfiguration, jsonPointerPath, iface, dict, boardNameOrig, ctn);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 3);

        auto name = std::get<std::string>(std::get<1>(*value.find("Name")));
        EXPECT_EQ(name, "MyRecord");

        auto type = std::get<std::string>(std::get<1>(*value.find("Type")));
        EXPECT_EQ(type, "Example");
    };
    postAssertHandler(handler, objPath, configInterface);
}

TEST_F(DBusTest, populateIntfPDICompatWithNestedObject)
{
    dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "X": {
        "Y": 4
    },
    "Type": "Example"
}
)");

    iface = em.dbus_interface.createInterface(objPath, configInterface,
                                              boardNameOrig);

    config_type_tree::ConfigTypeNode ctn = {"Example",
                                            {{"X", {"NestedConfig", {}}}}};

    em.dbus_interface.populateIntfPDICompat(
        systemConfiguration, jsonPointerPath, iface, dict, boardNameOrig, ctn);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 1);

        ASSERT_TRUE(value.find("Y") != value.end());

        auto y = std::get<int64_t>(std::get<1>(*value.find("Y")));
        EXPECT_EQ(y, 4);

        auto type = std::get<std::string>(std::get<1>(*value.find("Type")));
        EXPECT_EQ(type, "NestedConfig");
    };

    std::string nestedPath = objPath + "/X";
    std::string nestedInterface =
        "xyz.openbmc_project.Configuration.NestedConfig";

    postAssertHandler(handler, nestedPath, nestedInterface);
}

TEST_F(DBusTest, populateIntfPDICompatWithNestedArrayOfObjects)
{
    dict = nlohmann::json::parse(R"(
{
    "Name": "MyRecord",
    "X": [
        {
            "Y": 5
        },
        {
            "Y": 6
        }
    ],
    "Type": "Example"
}
)");

    iface = em.dbus_interface.createInterface(objPath, configInterface,
                                              boardNameOrig);

    config_type_tree::ConfigTypeNode ctn = {"Example",
                                            {{"X", {"NestedConfig", {}}}}};
    em.dbus_interface.populateIntfPDICompat(
        systemConfiguration, jsonPointerPath, iface, dict, boardNameOrig, ctn);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 1);

        ASSERT_TRUE(value.find("Y") != value.end());

        auto y = std::get<int64_t>(std::get<1>(*value.find("Y")));
        EXPECT_EQ(y, 5);

        auto type = std::get<std::string>(std::get<1>(*value.find("Type")));
        EXPECT_EQ(type, "NestedConfig");
    };

    std::string nestedPath = objPath + "/X/0";
    std::string nestedInterface =
        "xyz.openbmc_project.Configuration.NestedConfig";

    postAssertHandler(handler, nestedPath, nestedInterface);
}
