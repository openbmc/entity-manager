#include "fru_device/fru_utils.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <cstddef>
#include <memory>
#include <set>
#include <utility>

#include "gtest/gtest.h"

class FruDeviceRescanTest : public ::testing::Test
{
  protected:
    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> conn{
        std::make_shared<sdbusplus::asio::connection>(io)};
    std::unique_ptr<sdbusplus::asio::object_server> objServer;

    void SetUp() override
    {
        objServer =
            std::make_unique<sdbusplus::asio::object_server>(conn, true);
    }
};

TEST_F(FruDeviceRescanTest, InterfacesRemainPublishedDuringScanAndPrunedAfter)
{
    DBusIntfMap foundDevices;

    foundDevices.emplace(std::make_pair(1, 0x50),
                         createProbeInterface(1, 0x50, *objServer));
    foundDevices.emplace(std::make_pair(1, 0x51),
                         createProbeInterface(1, 0x51, *objServer));
    EXPECT_EQ(foundDevices.size(), 2);
    EXPECT_TRUE(foundDevices.contains(std::make_pair(1, 0x50)));
    EXPECT_TRUE(foundDevices.contains(std::make_pair(1, 0x51)));

    // Simulate in-progress scan where only 0x50 responds and 0x51 is missing.
    std::set<std::pair<size_t, size_t>> probedDevices;
    probedDevices.emplace(1, 0x50);

    // Existing interfaces must remain published while the scan is in progress.
    EXPECT_EQ(foundDevices.size(), 2);
    EXPECT_TRUE(foundDevices.contains(std::make_pair(1, 0x50)));
    EXPECT_TRUE(foundDevices.contains(std::make_pair(1, 0x51)));

    pruneFoundDevices(foundDevices, *objServer,
                      [&probedDevices](const auto& dev) {
                          return !probedDevices.contains(dev);
                      });

    EXPECT_EQ(foundDevices.size(), 1);
    EXPECT_TRUE(foundDevices.contains(std::make_pair(1, 0x50)));
    EXPECT_FALSE(foundDevices.contains(std::make_pair(1, 0x51)));
}

TEST_F(FruDeviceRescanTest, RescanOneBusOnlyPrunesTargetBus)
{
    DBusIntfMap foundDevices;

    foundDevices.emplace(std::make_pair(1, 0x50),
                         createProbeInterface(1, 0x50, *objServer));
    foundDevices.emplace(std::make_pair(2, 0x50),
                         createProbeInterface(2, 0x50, *objServer));
    EXPECT_EQ(foundDevices.size(), 2);

    std::set<std::pair<size_t, size_t>> probedDevices;
    constexpr size_t rescanBus = 1;

    pruneFoundDevices(
        foundDevices, *objServer, [rescanBus, &probedDevices](const auto& dev) {
            return dev.first == rescanBus && !probedDevices.contains(dev);
        });

    EXPECT_EQ(foundDevices.size(), 1);
    EXPECT_FALSE(foundDevices.contains(std::make_pair(1, 0x50)));
    EXPECT_TRUE(foundDevices.contains(std::make_pair(2, 0x50)));
}
