#include "assert_has_property.hpp"
#include "dbus_test.hpp"
#include "test_em.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include <gtest/gtest.h>

// synchronize initialization
std::counting_semaphore mInitMock(0);

static void mockFruDevice(const std::string& intf)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    sdbusplus::asio::object_server objServer(systemBus, true);
    objServer.add_manager("/xyz/openbmc_project/FruDevice");

    auto ifacePtr = objServer.add_interface(
        "/xyz/openbmc_project/FruDevice/ExampleFru", intf);

    // The type is important here
    // .BUS                                property  u         2
    // .ADDRESS                            property  u         80
    const uint32_t bus = 3;
    const uint32_t address = 80;

    std::string productName = "Wailua Falls";
    ifacePtr->register_property("BOARD_PRODUCT_NAME", productName.c_str());
    ifacePtr->register_property("BUS", bus);
    ifacePtr->register_property("ADDRESS", address);
    ifacePtr->initialize();

    systemBus->request_name("xyz.openbmc_project.FruDevice");

    for (int i = 0; i < 10; i++)
    {
        io.run_one();
    }

    mInitMock.release();

    io.run();
}

static void testProbeIntfSingleConfigNameAndType()
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    mInitMock.acquire();

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
       {
           "Name": "RecordName",
           "Bus": "$bus",
           "Address": "$address",
           "Type": "RecordType"
       }
   ],
   "Name": "ExampleBoard",
   "Probe": "xyz.openbmc_project.FruDevice1({'BOARD_PRODUCT_NAME': 'Wailua Falls'}) ",
   "Type": "Board"
}
)");

    // TODO: the test is failing since EM tries to use the mapper which is not
    // available <3> Error communicating to mapper. To run this test locally, an
    // instance of the mapper must be running

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        ASSERT_EQ(value.size(), 4);

        assertHasProperty<std::string>(value, "Name", "RecordName");
        assertHasProperty<std::string>(value, "Type", "RecordType");
        assertHasProperty<uint64_t>(value, "Bus", 3);
        assertHasProperty<uint64_t>(value, "Address", 80);
    };

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/RecordName");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.RecordType";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100, false);
}

TEST(EndToEndIntf, probeIntfSingleConfigNameAndType)
{
    std::thread t1(mockFruDevice, "xyz.openbmc_project.FruDevice1");
    t1.detach();

    std::thread t2(testProbeIntfSingleConfigNameAndType);
    t2.join();
}

static void testProbeIntfSingleConfigExpressionsWith1Var()
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    mInitMock.acquire();

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
       {
           "Name": "RecordName",
           "PropertyAdd": "$bus + 3",
           "PropertySub": "$address - 3",
           "PropertyMul": "$bus * 3",
           "PropertyDiv": "$address / 3",
           "Type": "RecordType"
       }
   ],
   "Name": "ExampleBoard",
   "Probe": "xyz.openbmc_project.FruDevice2({'BOARD_PRODUCT_NAME': 'Wailua Falls'}) ",
   "Type": "Board"
}
)");
    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        ASSERT_EQ(value.size(), 6);

        assertHasProperty<uint64_t>(value, "PropertyAdd", 6);
        assertHasProperty<uint64_t>(value, "PropertySub", 77);
        assertHasProperty<uint64_t>(value, "PropertyMul", 9);
        assertHasProperty<uint64_t>(value, "PropertyDiv", 26);
    };

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/RecordName");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.RecordType";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100, false);
}

TEST(EndToEndIntf, probeIntfSingleConfigExpressionsWith1Var)
{
    std::thread t1(mockFruDevice, "xyz.openbmc_project.FruDevice2");
    t1.detach();

    std::thread t2(testProbeIntfSingleConfigExpressionsWith1Var);
    t2.join();
}
