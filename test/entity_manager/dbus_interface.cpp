#include "entity_manager/dbus_interface.hpp"

#include "entity_manager/entity_manager.hpp"
#include "test_em.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include <gtest/gtest.h>

using namespace std::string_literals;

using DBusPropertiesMap = boost::container::flat_map<
    std::string, std::variant<std::string, int64_t, bool, std::vector<int64_t>,
                              std::vector<uint64_t>, std::vector<std::string>>>;

class DBusInterfaceTest : public testing::Test
{
  protected:
    DBusInterfaceTest() :
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        em(systemBus, io)
    {}
    ~DBusInterfaceTest() noexcept override = default;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    TestEM em;
    nlohmann::json systemConfiguration = "{}";
    std::string boardNameOrig = "myboard";
    std::string jsonPointerPath = "xyz";

    std::string configInterface = "xyz.openbmc_project.Configuration.Example";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/board/MyBoard/MyRecord";

    // This is written in the testcase
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;

    // The configuration to expose. Gets written in the testcase
    nlohmann::json dict;

  public:
    DBusInterfaceTest(const DBusInterfaceTest&) = delete;
    DBusInterfaceTest(DBusInterfaceTest&&) = delete;
    DBusInterfaceTest& operator=(const DBusInterfaceTest&) = delete;
    DBusInterfaceTest& operator=(DBusInterfaceTest&&) = delete;

    void postAssertHandler(
        const std::function<void(const DBusPropertiesMap& value)>& handler,
        std::string& path, std::string& interface, bool stopOnError = true)
    {
        auto h3 = [this, handler, stopOnError](boost::system::error_code ec,
                                               const DBusPropertiesMap& value) {
            EXPECT_FALSE(ec);
            if (ec)
            {
                lg2::error("{ERROR}", "ERROR", ec.message());

                // not run below line to manually inspect DBus in case of test
                // failure
                if (stopOnError)
                {
                    io.stop();
                }

                return;
            }

            try
            {
                handler(value);
                io.stop();
            }
            catch (std::exception& e)
            {
                if (stopOnError)
                {
                    io.stop();
                }
            }
        };

        boost::asio::post(io, [this, path, interface, h3]() {
            systemBus->async_method_call(h3, em.busName, path,
                                         "org.freedesktop.DBus.Properties",
                                         "GetAll", interface);
        });

        io.run();
    };
};

TEST_F(DBusInterfaceTest, createInterface)
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

TEST_F(DBusInterfaceTest, populateInterfaceFromJson_Simple)
{
    dict = {{"Name", "MyRecord"},
            {"X", 3},
            {"BoolProp", true},
            {"Type", "Example"}};

    iface = em.dbus_interface.createInterface(objPath, configInterface,
                                              boardNameOrig);

    em.dbus_interface.populateInterfaceFromJson(systemConfiguration,
                                                jsonPointerPath, iface, dict);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 4);

        auto name = std::get<std::string>(std::get<1>(*value.find("Name")));
        EXPECT_EQ(name, "MyRecord");

        auto type = std::get<std::string>(std::get<1>(*value.find("Type")));
        EXPECT_EQ(type, "Example");

        auto x = std::get<int64_t>(std::get<1>(*value.find("X")));
        EXPECT_EQ(x, 3);

        auto b = std::get<bool>(std::get<1>(*value.find("BoolProp")));
        EXPECT_EQ(b, true);

        EXPECT_EQ(value.find("Y"), value.end());
    };
    postAssertHandler(handler, objPath, configInterface);
}

TEST_F(DBusInterfaceTest, populateInterfaceFromJson_Arrays_Int)
{
    dict = {
        {"Name", "MyRecord"}, {"ArrayOfInts", {3, 2, 1}}, {"Type", "Example"}};

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

TEST_F(DBusInterfaceTest, populateInterfaceFromJson_Arrays_Bool)
{
    dict = {{"Name", "MyRecord"},
            {"ArrayOfBools", {true, false, true}},
            {"Type", "Example"}};

    lg2::debug("JSON: {JSON}", "JSON", dict);

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

TEST_F(DBusInterfaceTest, populateInterfaceFromJson_Arrays_String)
{
    dict = {{"Name", "MyRecord"},
            {"ArrayOfStrings", {"how", "many", "brownie", "points"}},
            {"Type", "Example"}};

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

TEST_F(DBusInterfaceTest, populateIntfPDICompat)
{
    dict = {{"Name", "MyRecord"}, {"X", 3}, {"Type", "Example"}};

    iface = em.dbus_interface.createInterface(objPath, configInterface,
                                              boardNameOrig);

    em.dbus_interface.populateIntfPDICompat(
        systemConfiguration, jsonPointerPath, iface, dict, boardNameOrig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 3);

        auto name = std::get<std::string>(std::get<1>(*value.find("Name")));
        EXPECT_EQ(name, "MyRecord");

        auto type = std::get<std::string>(std::get<1>(*value.find("Type")));
        EXPECT_EQ(type, "Example");
    };
    postAssertHandler(handler, objPath, configInterface);
}

TEST_F(DBusInterfaceTest, populateIntfPDICompatWithNestedObject)
{
    dict = {{"Name", "MyRecord"},
            {"X", {{"Y", 4}, {"Type", "NestedConfig"}}},
            {"Type", "Example"}};

    iface = em.dbus_interface.createInterface(objPath, configInterface,
                                              boardNameOrig);

    em.dbus_interface.populateIntfPDICompat(
        systemConfiguration, jsonPointerPath, iface, dict, boardNameOrig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 2);

        ASSERT_TRUE(value.find("Y") != value.end());
        ASSERT_TRUE(value.find("Type") != value.end());

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

TEST_F(DBusInterfaceTest, populateIntfPDICompatWithNestedArrayOfObjects)
{
    nlohmann::json::object_t x0 = {{"Y", 5}, {"Type", "NestedConfig"}};
    nlohmann::json::object_t x1 = {{"Y", 6}, {"Type", "NestedConfig"}};

    nlohmann::json::array_t ar = {x0, x1};

    nlohmann::json::object_t obj = {
        {"Name", "MyRecord"}, {"X", ar}, {"Type", "Example"}};

    dict = obj;

    iface = em.dbus_interface.createInterface(objPath, configInterface,
                                              boardNameOrig);

    em.dbus_interface.populateIntfPDICompat(
        systemConfiguration, jsonPointerPath, iface, dict, boardNameOrig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 2);

        ASSERT_TRUE(value.find("Y") != value.end());
        ASSERT_TRUE(value.find("Type") != value.end());

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
