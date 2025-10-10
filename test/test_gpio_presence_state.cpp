#include "gpio-presence/device_presence.hpp"
#include "gpio-presence/gpio_presence_manager.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace gpio_presence;

class DevicePresenceDetailedTest : public ::testing::Test
{
  protected:
    DevicePresenceDetailedTest() {}
    virtual ~DevicePresenceDetailedTest() noexcept {}

    sdbusplus::async::context ctx;
    std::unordered_map<std::string, bool> gpioState;
};

// Test DevicePresence constructor with various configurations
TEST_F(DevicePresenceDetailedTest, ConstructorTests)
{
    // Test 1: Single GPIO, active low
    {
        std::vector<std::string> gpioNames = {"GPIO1"};
        std::vector<uint64_t> gpioValues = {0}; // Active low
        std::string deviceName = "device1";

        DevicePresence device(ctx, gpioNames, gpioValues, deviceName,
                              gpioState);

        EXPECT_EQ(device.deviceName, deviceName);
        EXPECT_EQ(device.gpioPolarity.size(), 1);
        EXPECT_EQ(device.gpioPolarity["GPIO1"], ACTIVE_LOW);
    }

    // Test 2: Single GPIO, active high
    {
        std::vector<std::string> gpioNames = {"GPIO2"};
        std::vector<uint64_t> gpioValues = {1}; // Active high
        std::string deviceName = "device2";

        DevicePresence device(ctx, gpioNames, gpioValues, deviceName,
                              gpioState);

        EXPECT_EQ(device.deviceName, deviceName);
        EXPECT_EQ(device.gpioPolarity.size(), 1);
        EXPECT_EQ(device.gpioPolarity["GPIO2"], ACTIVE_HIGH);
    }

    // Test 3: Multiple GPIOs with mixed polarities
    {
        std::vector<std::string> gpioNames = {"GPIO1", "GPIO2", "GPIO3"};
        std::vector<uint64_t> gpioValues = {0, 1, 0}; // Active low, high, low
        std::string deviceName = "device3";

        DevicePresence device(ctx, gpioNames, gpioValues, deviceName,
                              gpioState);

        EXPECT_EQ(device.deviceName, deviceName);
        EXPECT_EQ(device.gpioPolarity.size(), 3);
        EXPECT_EQ(device.gpioPolarity["GPIO1"], ACTIVE_LOW);
        EXPECT_EQ(device.gpioPolarity["GPIO2"], ACTIVE_HIGH);
        EXPECT_EQ(device.gpioPolarity["GPIO3"], ACTIVE_LOW);
    }
}

// Test DevicePresence isPresent method with various GPIO states
TEST_F(DevicePresenceDetailedTest, IsPresentTests)
{
    // Test 1: Active low GPIO is low (device present)
    {
        std::unordered_map<std::string, bool> localGpioState;
        std::vector<std::string> gpioNames = {"GPIO1"};
        std::vector<uint64_t> gpioValues = {0}; // Active low
        std::string deviceName = "device1";

        localGpioState["GPIO1"] = false; // GPIO is low

        DevicePresence device(ctx, gpioNames, gpioValues, deviceName,
                              localGpioState);
        EXPECT_TRUE(device.isPresent());
    }

    // Test 2: Active low GPIO is high (device absent)
    {
        std::unordered_map<std::string, bool> localGpioState;
        std::vector<std::string> gpioNames = {"GPIO1"};
        std::vector<uint64_t> gpioValues = {0}; // Active low
        std::string deviceName = "device1";

        localGpioState["GPIO1"] = true; // GPIO is high

        DevicePresence device(ctx, gpioNames, gpioValues, deviceName,
                              localGpioState);
        EXPECT_FALSE(device.isPresent());
    }

    // Test 3: Active high GPIO is high (device present)
    {
        std::unordered_map<std::string, bool> localGpioState;
        std::vector<std::string> gpioNames = {"GPIO1"};
        std::vector<uint64_t> gpioValues = {1}; // Active high
        std::string deviceName = "device1";

        localGpioState["GPIO1"] = true; // GPIO is high

        DevicePresence device(ctx, gpioNames, gpioValues, deviceName,
                              localGpioState);
        EXPECT_TRUE(device.isPresent());
    }

    // Test 4: Active high GPIO is low (device absent)
    {
        std::unordered_map<std::string, bool> localGpioState;
        std::vector<std::string> gpioNames = {"GPIO1"};
        std::vector<uint64_t> gpioValues = {1}; // Active high
        std::string deviceName = "device1";

        localGpioState["GPIO1"] = false; // GPIO is low

        DevicePresence device(ctx, gpioNames, gpioValues, deviceName,
                              localGpioState);
        EXPECT_FALSE(device.isPresent());
    }

    // Test 5: Multiple GPIOs all correct (device present)
    {
        std::unordered_map<std::string, bool> localGpioState;
        std::vector<std::string> gpioNames = {"GPIO1", "GPIO2", "GPIO3"};
        std::vector<uint64_t> gpioValues = {0, 1, 0}; // Active low, high, low
        std::string deviceName = "device1";

        localGpioState["GPIO1"] = false; // Active low, should be low
        localGpioState["GPIO2"] = true;  // Active high, should be high
        localGpioState["GPIO3"] = false; // Active low, should be low

        DevicePresence device(ctx, gpioNames, gpioValues, deviceName,
                              localGpioState);
        EXPECT_TRUE(device.isPresent());
    }

    // Test 6: Multiple GPIOs one incorrect (device absent)
    {
        std::unordered_map<std::string, bool> localGpioState;
        std::vector<std::string> gpioNames = {"GPIO1", "GPIO2", "GPIO3"};
        std::vector<uint64_t> gpioValues = {0, 1, 0}; // Active low, high, low
        std::string deviceName = "device1";

        localGpioState["GPIO1"] = false; // Active low, should be low - correct
        localGpioState["GPIO2"] =
            false; // Active high, should be high - incorrect
        localGpioState["GPIO3"] = false; // Active low, should be low - correct

        DevicePresence device(ctx, gpioNames, gpioValues, deviceName,
                              localGpioState);
        EXPECT_FALSE(device.isPresent());
    }

    // Test 7: Missing GPIO state (device absent)
    {
        std::unordered_map<std::string, bool> localGpioState;
        std::vector<std::string> gpioNames = {"GPIO1"};
        std::vector<uint64_t> gpioValues = {0}; // Active low
        std::string deviceName = "device1";

        // localGpioState["GPIO1"] is not set - simulating missing GPIO

        DevicePresence device(ctx, gpioNames, gpioValues, deviceName,
                              localGpioState);
        EXPECT_FALSE(device.isPresent());
    }
}

// Test DevicePresence getObjPath method
TEST_F(DevicePresenceDetailedTest, GetObjPathTest)
{
    std::vector<std::string> gpioNames = {"GPIO1"};
    std::vector<uint64_t> gpioValues = {0};
    std::string deviceName = "test_device";

    DevicePresence device(ctx, gpioNames, gpioValues, deviceName, gpioState);

    sdbusplus::message::object_path objPath = device.getObjPath();
    std::string expectedPath =
        "/xyz/openbmc_project/GPIODeviceDetected/" + deviceName;

    EXPECT_EQ(objPath.str, expectedPath);
}
