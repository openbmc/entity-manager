#include "gpio-presence/config.hpp"
#include "gpio-presence/gpio_presence.hpp"

#include <gpiod.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Inventory/Source/DevicePresence/client.hpp>

#include <gtest/gtest.h>

using namespace gpio_presence;

// NOLINTBEGIN
sdbusplus::async::task<> request_stop(sdbusplus::async::context& io)
// NOLINTEND
{
    io.request_stop();
    co_return;
}

TEST(GpioPresence, ConstructionSucceeds)
{
    sdbusplus::async::context ctx;

    gpio_presence::GPIOPresence s(ctx);

    ctx.spawn(request_stop(ctx));
    ctx.run();
}

TEST(GpioPresence, AcceptConfig1Gpio)
{
    sdbusplus::async::context ctx;

    gpio_presence::GPIOPresence sensor(ctx);

    std::string name = "cable0";
    std::string gpioName = "TEST_GPIO";

    std::vector<std::string> gpioNames = {gpioName};
    std::vector<uint64_t> gpioValues = {0};

    auto c = std::make_unique<gpio_presence::Config>(ctx, gpioNames, gpioValues,
                                                     name);

    sensor.addConfig(name, std::move(c));

    sensor.updatePresence(gpioName, false);

    EXPECT_EQ(sensor.getPresence(name), true);

    sensor.updatePresence(gpioName, true);

    EXPECT_EQ(sensor.getPresence(name), false);

    ctx.spawn(request_stop(ctx));
    ctx.run();
}

// NOLINTBEGIN
sdbusplus::async::task<> testDevicePresentDbus(sdbusplus::async::context& ctx)
// NOLINTEND
{
    gpio_presence::GPIOPresence sensor(ctx);

    std::string busName = sensor.setupBusName();

    std::string name = "cable0";
    std::string gpioName = "TEST_GPIO";

    std::vector<std::string> gpioNames = {gpioName};
    std::vector<uint64_t> gpioValues = {0};

    auto c = std::make_unique<gpio_presence::Config>(ctx, gpioNames, gpioValues,
                                                     name);

    sdbusplus::message::object_path objPath = c->getObjPath();

    sensor.addConfig(name, std::move(c));

    sensor.updatePresence(gpioName, false);

    lg2::debug("found obj path {OBJPATH}", "OBJPATH", objPath);

    auto client = sdbusplus::client::xyz::openbmc_project::inventory::source::
                      DevicePresence<>(ctx)
                          .service(busName)
                          .path(objPath.str);

    std::string nameFound = co_await client.name();

    assert(nameFound == "cable0");

    ctx.request_stop();

    co_return;
}

TEST(GpioPresence, DevicePresentDbus)
{
    sdbusplus::async::context ctx;
    ctx.spawn(testDevicePresentDbus(ctx));
    ctx.run();
}

// NOLINTBEGIN
sdbusplus::async::task<> testDevicePresentThenDisappearDbus(
    sdbusplus::async::context& ctx)
// NOLINTEND
{
    gpio_presence::GPIOPresence sensor(ctx);

    std::string busName = sensor.setupBusName();

    std::string name = "cable0";
    std::string gpioName = "TEST_GPIO";

    std::vector<std::string> gpioNames = {gpioName};
    std::vector<uint64_t> gpioValues = {0};

    auto c = std::make_unique<gpio_presence::Config>(ctx, gpioNames, gpioValues,
                                                     name);

    sdbusplus::message::object_path objPath = c->getObjPath();

    sensor.addConfig(name, std::move(c));

    sensor.updatePresence(gpioName, false);

    lg2::debug("found obj path {OBJPATH}", "OBJPATH", objPath);

    auto client = sdbusplus::client::xyz::openbmc_project::inventory::source::
                      DevicePresence<>(ctx)
                          .service(busName)
                          .path(objPath.str);

    std::string nameFound = co_await client.name();

    assert(nameFound == "cable0");

    // gpio goes high, cable 0 should disappear
    sensor.updatePresence(gpioName, true);

    try
    {
        co_await client.name();
        assert(false);
    }
    catch (std::exception& _)
    {
        // expected, since cable 0 is gone.
        // have to do something here to shut up clang-tidy
        std::cout << "" << std::endl;
    }

    ctx.request_stop();

    co_return;
}

TEST(GpioPresence, DevicePresentThenDisappearDbus)
{
    sdbusplus::async::context ctx;
    ctx.spawn(testDevicePresentThenDisappearDbus(ctx));
    ctx.run();
}
