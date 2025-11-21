#include "assert_has_property.hpp"
#include "dbus_test.hpp"
#include "test_em.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include <gtest/gtest.h>

static void checkProbeTrueSingleConfigNameAndType(const DBusInterface& value)
{
    ASSERT_EQ(value.size(), 2);

    assertHasProperty<std::string>(value, "Name", "RecordName");
    assertHasProperty<std::string>(value, "Type", "RecordType");
}

TEST(EndToEnd, probeTrueSingleConfigNameAndType)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    nlohmann::json::object_t exampleConfig;

    exampleConfig["Probe"] = "TRUE";
    exampleConfig["Type"] = "Board";
    exampleConfig["Name"] = "ExampleBoard";

    nlohmann::json::object_t record1;

    record1["Name"] = "RecordName";
    record1["Type"] = "RecordType";

    exampleConfig["Exposes"] = {record1};

    TestEM em(systemBus, io, exampleConfig);

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/RecordName");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.RecordType";
    DBusTest::staticPostAssertHandler(
        io, systemBus, checkProbeTrueSingleConfigNameAndType, em.busName,
        objPath, configInterface, 100);
}

static void checkProbeTrueSingleConfigIntProperty(const DBusInterface& value)
{
    EXPECT_EQ(value.size(), 4);

    assertHasProperty<uint64_t>(value, "X1", 3);
    assertHasProperty<int64_t>(value, "X2", -3);
};

TEST(EndToEnd, probeTrueSingleConfigIntProperty)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    nlohmann::json::object_t exampleConfig;

    exampleConfig["Probe"] = "TRUE";
    exampleConfig["Type"] = "Board";
    exampleConfig["Name"] = "ExampleBoard";

    nlohmann::json::object_t record1;

    record1["Name"] = "ExampleRecord";
    record1["Type"] = "Example";
    record1["X1"] = 3;
    record1["X2"] = -3;

    exampleConfig["Exposes"] = {record1};

    TestEM em(systemBus, io, exampleConfig);

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleRecord");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example";
    DBusTest::staticPostAssertHandler(
        io, systemBus, checkProbeTrueSingleConfigIntProperty, em.busName,
        objPath, configInterface, 100);
}

static void checkProbeTrueSingleConfigDoubleProperty(const DBusInterface& value)
{
    EXPECT_EQ(value.size(), 3);

    assertHasProperty<double>(value, "X3", 2.0);
};

TEST(EndToEnd, probeTrueSingleConfigDoubleProperty)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    nlohmann::json::object_t exampleConfig;

    exampleConfig["Probe"] = "TRUE";
    exampleConfig["Type"] = "Board";
    exampleConfig["Name"] = "ExampleBoard";

    nlohmann::json::object_t record1;

    record1["Name"] = "ExampleRecord";
    record1["Type"] = "Example";
    record1["X3"] = 2.0;

    exampleConfig["Exposes"] = {record1};

    TestEM em(systemBus, io, exampleConfig);

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleRecord");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example";
    DBusTest::staticPostAssertHandler(
        io, systemBus, checkProbeTrueSingleConfigDoubleProperty, em.busName,
        objPath, configInterface, 100);
}

static void checkProbeTrueSingleConfigBoolProperty(const DBusInterface& value)
{
    EXPECT_EQ(value.size(), 3);

    assertHasProperty<bool>(value, "BoolProp", false);
};

TEST(EndToEnd, probeTrueSingleConfigBoolProperty)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    nlohmann::json::object_t exampleConfig;

    exampleConfig["Probe"] = "TRUE";
    exampleConfig["Type"] = "Board";
    exampleConfig["Name"] = "ExampleBoard";

    nlohmann::json::object_t record1;

    record1["Name"] = "ExampleRecord";
    record1["Type"] = "Example";
    record1["BoolProp"] = false;

    exampleConfig["Exposes"] = {record1};

    TestEM em(systemBus, io, exampleConfig);

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleRecord");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example";
    DBusTest::staticPostAssertHandler(
        io, systemBus, checkProbeTrueSingleConfigBoolProperty, em.busName,
        objPath, configInterface, 100);
}

static void checkProbeTrueSingleConfigArrayOfStrings(const DBusInterface& value)
{
    EXPECT_EQ(value.size(), 3);

    assertHasProperty<std::vector<std::string>>(value, "ArrayOfStrings",
                                                {"String1", "String2"});
};

TEST(EndToEnd, probeTrueSingleConfigArrayOfStrings)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    nlohmann::json::object_t exampleConfig;

    exampleConfig["Probe"] = "TRUE";
    exampleConfig["Type"] = "Board";
    exampleConfig["Name"] = "ExampleBoard";

    nlohmann::json::object_t record1;

    record1["Name"] = "ExampleRecord";
    record1["Type"] = "Example";
    record1["ArrayOfStrings"] = {"String1", "String2"};

    exampleConfig["Exposes"] = {record1};

    TestEM em(systemBus, io, exampleConfig);

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleRecord");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example";
    DBusTest::staticPostAssertHandler(
        io, systemBus, checkProbeTrueSingleConfigArrayOfStrings, em.busName,
        objPath, configInterface, 100);
}

static void checkProbeTrueSingleConfigArrayOfInts(const DBusInterface& value)
{
    EXPECT_EQ(value.size(), 3);

    assertHasProperty<std::vector<uint64_t>>(value, "ArrayOfInts", {1, 2, 3});
};

TEST(EndToEnd, probeTrueSingleConfigArrayOfInts)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    nlohmann::json::object_t exampleConfig;

    exampleConfig["Probe"] = "TRUE";
    exampleConfig["Type"] = "Board";
    exampleConfig["Name"] = "ExampleBoard";

    nlohmann::json::object_t record1;

    record1["Name"] = "ExampleRecord";
    record1["Type"] = "Example";
    record1["ArrayOfInts"] = {1, 2, 3};

    exampleConfig["Exposes"] = {record1};

    TestEM em(systemBus, io, exampleConfig);

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleRecord");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example";
    DBusTest::staticPostAssertHandler(
        io, systemBus, checkProbeTrueSingleConfigArrayOfInts, em.busName,
        objPath, configInterface, 100);
}

static void checkProbeTrueSingleConfigArrayOfDoubles(const DBusInterface& value)
{
    EXPECT_EQ(value.size(), 3);

    assertHasProperty<std::vector<double>>(value, "ArrayOfDoubles",
                                           {1.0, 2.0, 3.0});
};

TEST(EndToEnd, probeTrueSingleConfigArrayOfDoubles)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    nlohmann::json::object_t exampleConfig;

    exampleConfig["Probe"] = "TRUE";
    exampleConfig["Type"] = "Board";
    exampleConfig["Name"] = "ExampleBoard";

    nlohmann::json::object_t record1;

    record1["Name"] = "ExampleRecord";
    record1["Type"] = "Example";
    record1["ArrayOfDoubles"] = {1.0, 2.0, 3.0};

    exampleConfig["Exposes"] = {record1};

    TestEM em(systemBus, io, exampleConfig);

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/ExampleRecord");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example";
    DBusTest::staticPostAssertHandler(
        io, systemBus, checkProbeTrueSingleConfigArrayOfDoubles, em.busName,
        objPath, configInterface, 100);
}

static void checkProbeTrueSingleConfigNestedObject(const DBusInterface& value)
{
    EXPECT_EQ(value.size(), 2);

    assertHasProperty<std::string>(value, "CompatibleHardware",
                                   "com.tyan.Hardware.S8030.SPI.Host");

    assertHasProperty<uint64_t>(value, "VendorIANA", 6653);
};

TEST(EndToEnd, probeTrueSingleConfigNestedObject)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    nlohmann::json::object_t exampleConfig;

    exampleConfig["Probe"] = "TRUE";
    exampleConfig["Type"] = "Board";
    exampleConfig["Name"] = "ExampleBoard";

    nlohmann::json::object_t record1;

    record1["Name"] = "HostSPIFlash";
    record1["Type"] = "SPIFlash";
    record1["SPIControllerIndex"] = 1;
    record1["SPIDeviceIndex"] = 0;

    nlohmann::json::object_t muxOutput;
    muxOutput["Name"] = "BMC_SPI_SEL";
    muxOutput["Polarity"] = "High";

    nlohmann::json::object_t firmwareInfo;
    firmwareInfo["CompatibleHardware"] = "com.tyan.Hardware.S8030.SPI.Host";
    firmwareInfo["VendorIANA"] = 6653;

    record1["MuxOutputs"] = {muxOutput};
    record1["FirmwareInfo"] = firmwareInfo;

    exampleConfig["Exposes"] = {record1};

    TestEM em(systemBus, io, exampleConfig);

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/HostSPIFlash");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.SPIFlash.FirmwareInfo";
    DBusTest::staticPostAssertHandler(
        io, systemBus, checkProbeTrueSingleConfigNestedObject, em.busName,
        objPath, configInterface, 100);
}

static void checkProbeTrueSingleConfigArrayOfObjects(const DBusInterface& value)
{
    EXPECT_EQ(value.size(), 5);

    assertHasProperty<std::string>(value, "Label", "temp1");
    assertHasProperty<double>(value, "Severity", 0);
    assertHasProperty<double>(value, "Value", 80.0);
};

TEST(EndToEnd, probeTrueSingleConfigArrayOfObjects)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    nlohmann::json::object_t exampleConfig;

    exampleConfig["Probe"] = "TRUE";
    exampleConfig["Type"] = "Board";
    exampleConfig["Name"] = "ExampleBoard";

    nlohmann::json::object_t record1;

    record1["Name"] = "CPU0_Temp";
    record1["Type"] = "TMP75";
    record1["Address"] = "0x4c";
    record1["Bus"] = 0;

    nlohmann::json::object_t threshold0;
    threshold0["Direction"] = "greater than";
    threshold0["Label"] = "temp1";
    threshold0["Name"] = "upper non critical";
    threshold0["Severity"] = 0;
    threshold0["Value"] = 80;

    nlohmann::json::object_t threshold1;
    threshold1["Direction"] = "greater than";
    threshold1["Label"] = "temp1";
    threshold1["Name"] = "upper critical";
    threshold1["Severity"] = 1;
    threshold1["Value"] = 95;

    record1["Thresholds"] = {threshold0, threshold1};

    exampleConfig["Exposes"] = {record1};

    TestEM em(systemBus, io, exampleConfig);

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/CPU0_Temp");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.TMP75.Thresholds0";
    DBusTest::staticPostAssertHandler(
        io, systemBus, checkProbeTrueSingleConfigArrayOfObjects, em.busName,
        objPath, configInterface, 100);
}

static void checkProbeTrueSingleConfigTemplateParamIndex(
    const DBusInterface& value)
{
    EXPECT_EQ(value.size(), 5);

    assertHasProperty<std::string>(value, "Name1", "CPU_Temp 1");
};

TEST(EndToEnd, probeTrueSingleConfigTemplateParamIndex)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    nlohmann::json::object_t exampleConfig;

    exampleConfig["Probe"] = "TRUE";
    exampleConfig["Type"] = "Board";
    exampleConfig["Name"] = "ExampleBoard";

    nlohmann::json::object_t record1;
    record1["Address"] = "0x4c";
    record1["Name"] = "Temp1";
    record1["Name1"] = "CPU_Temp $index";
    record1["Bus"] = 1;
    record1["Type"] = "ExampleType";

    nlohmann::json::object_t record2;
    record2["Address"] = "0x4c";
    record2["Name"] = "Temp2";
    record2["Name1"] = "CPU_Temp $index";
    record2["Bus"] = 2;
    record2["Type"] = "ExampleType";

    exampleConfig["Exposes"] = {record1, record2};

    TestEM em(systemBus, io, exampleConfig);

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/Temp1");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.ExampleType";
    DBusTest::staticPostAssertHandler(
        io, systemBus, checkProbeTrueSingleConfigTemplateParamIndex, em.busName,
        objPath, configInterface, 100);
}

static void checkProbeTrueSingleConfigTemplateParamIndexInObjPath(
    const DBusInterface& value)
{
    EXPECT_EQ(value.size(), 2);
};

TEST(EndToEnd, probeTrueSingleConfigTemplateParamIndexInObjPath1)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    nlohmann::json::object_t exampleConfig;

    exampleConfig["Probe"] = "TRUE";
    exampleConfig["Type"] = "Board";
    exampleConfig["Name"] = "ExampleBoard";

    nlohmann::json::object_t record1;
    record1["Name"] = "CPU Temp $index";
    record1["Type"] = "ExampleType";

    exampleConfig["Exposes"] = {record1};

    TestEM em(systemBus, io, exampleConfig);

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard/CPU_Temp_1");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.ExampleType";
    DBusTest::staticPostAssertHandler(
        io, systemBus, checkProbeTrueSingleConfigTemplateParamIndexInObjPath,
        em.busName, objPath, configInterface, 100);
}

static void checkProbeTrueArrayOfConfigs(const DBusInterface& value)
{
    EXPECT_EQ(value.size(), 3);

    assertHasProperty<std::vector<std::string>>(
        value, "ArrayOfStrings", {"FirstString", "SecondString"});
};

TEST(EndToEnd, probeTrueArrayOfConfigs)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    nlohmann::json::object_t exampleConfig1;

    exampleConfig1["Probe"] = "TRUE";
    exampleConfig1["Type"] = "Board";
    exampleConfig1["Name"] = "ExampleBoard0";

    nlohmann::json::object_t record1;
    record1["Name"] = "ExampleRecord1";
    record1["ArrayOfStrings"] = {"FirstString", "SecondString"};
    record1["Type"] = "Example2";

    exampleConfig1["Exposes"] = {record1};

    nlohmann::json::object_t exampleConfig2;

    exampleConfig2["Probe"] = "TRUE";
    exampleConfig2["Type"] = "Board";
    exampleConfig2["Name"] = "ExampleBoard1";

    nlohmann::json::array_t exampleConfig = {exampleConfig1, exampleConfig2};

    TestEM em(systemBus, io, exampleConfig);

    const sdbusplus::message::object_path objPath(
        "/xyz/openbmc_project/inventory/system/board/ExampleBoard0/ExampleRecord1");

    const std::string configInterface =
        "xyz.openbmc_project.Configuration.Example2";
    DBusTest::staticPostAssertHandler(io, systemBus,
                                      checkProbeTrueArrayOfConfigs, em.busName,
                                      objPath, configInterface, 100);
}
