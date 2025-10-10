#include "gpio-presence/device_presence.hpp"
#include "gpio-presence/gpio_presence_manager.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>

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

// Test DevicePresence constructor with single GPIO, active low
TEST_F(DevicePresenceDetailedTest, ConstructorSingleGpioActiveLow)
{
    std::vector<std::string> gpioNames = {"GPIO1"};
    std::vector<uint64_t> gpioValues = {0}; // Active low
    std::string deviceName = "device1";

    DevicePresence device(ctx, gpioNames, gpioValues, deviceName, gpioState);

    EXPECT_EQ(device.deviceName, deviceName);
    EXPECT_EQ(device.gpioPolarity.size(), 1);
    EXPECT_EQ(device.gpioPolarity["GPIO1"], ACTIVE_LOW);
}

// Test DevicePresence constructor with single GPIO, active high
TEST_F(DevicePresenceDetailedTest, ConstructorSingleGpioActiveHigh)
{
    std::vector<std::string> gpioNames = {"GPIO2"};
    std::vector<uint64_t> gpioValues = {1}; // Active high
    std::string deviceName = "device2";

    DevicePresence device(ctx, gpioNames, gpioValues, deviceName, gpioState);

    EXPECT_EQ(device.deviceName, deviceName);
    EXPECT_EQ(device.gpioPolarity.size(), 1);
    EXPECT_EQ(device.gpioPolarity["GPIO2"], ACTIVE_HIGH);
}

// Test DevicePresence constructor with multiple GPIOs with mixed polarities
TEST_F(DevicePresenceDetailedTest, ConstructorMultipleGpiosMixedPolarities)
{
    std::vector<std::string> gpioNames = {"GPIO1", "GPIO2", "GPIO3"};
    std::vector<uint64_t> gpioValues = {0, 1, 0}; // Active low, high, low
    std::string deviceName = "device3";

    DevicePresence device(ctx, gpioNames, gpioValues, deviceName, gpioState);

    EXPECT_EQ(device.deviceName, deviceName);
    EXPECT_EQ(device.gpioPolarity.size(), 3);
    EXPECT_EQ(device.gpioPolarity["GPIO1"], ACTIVE_LOW);
    EXPECT_EQ(device.gpioPolarity["GPIO2"], ACTIVE_HIGH);
    EXPECT_EQ(device.gpioPolarity["GPIO3"], ACTIVE_LOW);
}

// Test DevicePresence isPresent method with active low GPIO is low (device
// present)
TEST_F(DevicePresenceDetailedTest, IsPresentActiveLowGpioLow)
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

// Test DevicePresence isPresent method with active low GPIO is high (device
// absent)
TEST_F(DevicePresenceDetailedTest, IsPresentActiveLowGpioHigh)
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

// Test DevicePresence isPresent method with active high GPIO is high (device
// present)
TEST_F(DevicePresenceDetailedTest, IsPresentActiveHighGpioHigh)
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

// Test DevicePresence isPresent method with active high GPIO is low (device
// absent)
TEST_F(DevicePresenceDetailedTest, IsPresentActiveHighGpioLow)
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

// Test DevicePresence isPresent method with multiple GPIOs all correct (device
// present)
TEST_F(DevicePresenceDetailedTest, IsPresentMultipleGpiosAllCorrect)
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

// Test DevicePresence isPresent method with multiple GPIOs one incorrect
// (device absent)
TEST_F(DevicePresenceDetailedTest, IsPresentMultipleGpiosOneIncorrect)
{
    std::unordered_map<std::string, bool> localGpioState;
    std::vector<std::string> gpioNames = {"GPIO1", "GPIO2", "GPIO3"};
    std::vector<uint64_t> gpioValues = {0, 1, 0}; // Active low, high, low
    std::string deviceName = "device1";

    localGpioState["GPIO1"] = false; // Active low, should be low - correct
    localGpioState["GPIO2"] = false; // Active high, should be high - incorrect
    localGpioState["GPIO3"] = false; // Active low, should be low - correct

    DevicePresence device(ctx, gpioNames, gpioValues, deviceName,
                          localGpioState);
    EXPECT_FALSE(device.isPresent());
}

// Test DevicePresence isPresent method with missing GPIO state (device absent)
TEST_F(DevicePresenceDetailedTest, IsPresentMissingGpioState)
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
