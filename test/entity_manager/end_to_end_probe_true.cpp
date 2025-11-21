#include "assert_has_property.hpp"
#include "dbus_test.hpp"
#include "test_em.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include <gtest/gtest.h>

TEST(EndToEnd, probeTrueSingleConfigNameAndType)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
       {
           "Name": "RecordName",
           "Type": "RecordType"
       }
   ],
   "Name": "ExampleBoard",
   "Probe": "TRUE",
   "Type": "Board"
}
)");

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        ASSERT_EQ(value.size(), 2);

        assertHasProperty<std::string>(value, "Name", "RecordName");
        assertHasProperty<std::string>(value, "Type", "RecordType");
    };

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/RecordName");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.RecordType";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100);
}

TEST(EndToEnd, probeTrueSingleConfigIntProperty)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
       {
           "Name": "ExampleRecord",
           "X1": 3,
           "X2": -3,
           "Type": "Example"
       }
   ],
   "Name": "ExampleBoard",
   "Probe": "TRUE",
   "Type": "Board"
}
)");

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 4);

        assertHasProperty<uint64_t>(value, "X1", 3);
        assertHasProperty<int64_t>(value, "X2", -3);
    };
    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleRecord");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100);
}

TEST(EndToEnd, probeTrueSingleConfigDoubleProperty)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
       {
           "Name": "ExampleRecord",
           "X3": 2.0,
           "Type": "Example"
       }
   ],
   "Name": "ExampleBoard",
   "Probe": "TRUE",
   "Type": "Board"
}
)");

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 3);

        assertHasProperty<double>(value, "X3", 2.0);
    };
    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleRecord");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100);
}

TEST(EndToEnd, probeTrueSingleConfigBoolProperty)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
       {
           "Name": "ExampleRecord",
           "BoolProp": false,
           "Type": "Example"
       }
   ],
   "Name": "ExampleBoard",
   "Probe": "TRUE",
   "Type": "Board"
}
)");

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 3);

        assertHasProperty<bool>(value, "BoolProp", false);
    };

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleRecord");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100);
}

TEST(EndToEnd, probeTrueSingleConfigArrayOfStrings)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
       {
           "Name": "ExampleRecord",
           "ArrayOfStrings": ["String1", "String2"],
           "Type": "Example"
       }
   ],
   "Name": "ExampleBoard",
   "Probe": "TRUE",
   "Type": "Board"
}
)");

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 3);

        assertHasProperty<std::vector<std::string>>(value, "ArrayOfStrings",
                                                    {"String1", "String2"});
    };
    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleRecord");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100);
}

TEST(EndToEnd, probeTrueSingleConfigArrayOfInts)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
       {
           "Name": "ExampleRecord",
           "ArrayOfInts": [1, 2, 3],
           "Type": "Example"
       }
   ],
   "Name": "ExampleBoard",
   "Probe": "TRUE",
   "Type": "Board"
}
)");

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 3);

        assertHasProperty<std::vector<uint64_t>>(value, "ArrayOfInts",
                                                 {1, 2, 3});
    };
    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleRecord");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100);
}

TEST(EndToEnd, probeTrueSingleConfigArrayOfDoubles)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
       {
           "Name": "ExampleRecord",
           "ArrayOfDoubles": [1.0, 2.0, 3.0],
           "Type": "Example"
       }
   ],
   "Name": "ExampleBoard",
   "Probe": "TRUE",
   "Type": "Board"
}
)");

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 3);

        assertHasProperty<std::vector<double>>(value, "ArrayOfDoubles",
                                               {1.0, 2.0, 3.0});
    };
    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleRecord");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100);
}

TEST(EndToEnd, probeTrueSingleConfigNestedObject)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
        {
            "FirmwareInfo": {
                "CompatibleHardware": "com.tyan.Hardware.S8030.SPI.Host",
                "VendorIANA": 6653
            },
            "MuxOutputs": [
                {
                    "Name": "BMC_SPI_SEL",
                    "Polarity": "High"
                }
            ],
            "Name": "HostSPIFlash",
            "SPIControllerIndex": 1,
            "SPIDeviceIndex": 0,
            "Type": "SPIFlash"
        }
   ],
   "Name": "ExampleBoard",
   "Probe": "TRUE",
   "Type": "Board"
}
)");

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 2);

        assertHasProperty<std::string>(value, "CompatibleHardware",
                                       "com.tyan.Hardware.S8030.SPI.Host");

        assertHasProperty<uint64_t>(value, "VendorIANA", 6653);
    };
    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/HostSPIFlash");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.SPIFlash.FirmwareInfo";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100);
}

TEST(EndToEnd, probeTrueSingleConfigArrayOfObjects)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
       {
           "Address": "0x4c",
           "Name": "CPU0_Temp",
           "Bus": 0,
           "Thresholds": [
                {
                    "Direction": "greater than",
                    "Label": "temp1",
                    "Name": "upper non critical",
                    "Severity": 0,
                    "Value": 80
                },
                {
                    "Direction": "greater than",
                    "Label": "temp1",
                    "Name": "upper critical",
                    "Severity": 1,
                    "Value": 95
                }
           ],
           "Type": "TMP75"
       }
   ],
   "Name": "ExampleBoard",
   "Probe": "TRUE",
   "Type": "Board"
}
)");

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 5);

        assertHasProperty<std::string>(value, "Label", "temp1");
        assertHasProperty<double>(value, "Severity", 0);
        assertHasProperty<double>(value, "Value", 80.0);
    };
    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/CPU0_Temp");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.TMP75.Thresholds0";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100);
}

TEST(EndToEnd, probeTrueSingleConfigTemplateParamIndex)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
       {
           "Address": "0x4c",
           "Name": "Temp1",
           "Name1": "CPU_Temp $index",
           "Bus": 1,
           "Type": "ExampleType"
       },
       {
           "Address": "0x4c",
           "Name": "Temp2",
           "Name1": "CPU_Temp $index",
           "Bus": 2,
           "Type": "ExampleType"
       }
   ],
   "Name": "ExampleBoard",
   "Probe": "TRUE",
   "Type": "Board"
}
)");

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 5);

        assertHasProperty<std::string>(value, "Name1", "CPU_Temp 1");
    };
    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/Temp1");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.ExampleType";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100);
}

TEST(EndToEnd, probeTrueSingleConfigTemplateParamIndexInObjPath1)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    const auto exampleConfig = nlohmann::json::parse(R"(
{
    "Exposes": [
       {
           "Name": "CPU Temp $index",
           "Type": "ExampleType"
       }
   ],
   "Name": "ExampleBoard",
   "Probe": "TRUE",
   "Type": "Board"
}
)");

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 2);
    };
    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/CPU_Temp_1");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.ExampleType";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100);
}

TEST(EndToEnd, probeTrueArrayOfConfigs)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    const auto exampleConfig = nlohmann::json::parse(R"(
[
{
   "Exposes": [
       {
           "Name": "ExampleRecord1",
           "ArrayOfStrings": ["FirstString", "SecondString"],
           "Type": "Example2"
       }
   ],
   "Name": "ExampleBoard0",
   "Probe": "TRUE",
   "Type": "Board"
},
{
   "Exposes": [
   ],
   "Name": "ExampleBoard1",
   "Probe": "TRUE",
   "Type": "Board"
}
]
)");

    TestEM em(systemBus, io, exampleConfig);

    auto handler = [](const DBusPropertiesMap& value) {
        EXPECT_EQ(value.size(), 3);

        assertHasProperty<std::vector<std::string>>(
            value, "ArrayOfStrings", {"FirstString", "SecondString"});
    };
    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard0/ExampleRecord1");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example2";
    DBusTest::staticPostAssertHandler(io, systemBus, handler, em.busName,
                                      objPath, configInterface, 100);
}
