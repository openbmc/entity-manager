#include "fru_device/fru_utils.hpp"

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <cstddef>
#include <memory>
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

    auto iface50 = createProbeInterface(1, 0x50, *objServer);
    auto iface51 = createProbeInterface(1, 0x51, *objServer);
    std::weak_ptr<sdbusplus::asio::dbus_interface> weak50 = iface50;
    std::weak_ptr<sdbusplus::asio::dbus_interface> weak51 = iface51;

    foundDevices.emplace(std::make_pair(1, 0x50), std::move(iface50));
    foundDevices.emplace(std::make_pair(1, 0x51), std::move(iface51));

    bool scanExecuted = false;
    runRescan(foundDevices, *objServer, [&](ScanResult& result) {
        scanExecuted = true;

        // While scan is in progress, existing interfaces must remain published.
        EXPECT_EQ(foundDevices.size(), 2);
        EXPECT_FALSE(weak50.expired());
        EXPECT_FALSE(weak51.expired());

        result.probedDevices.emplace(1, 0x50);
    });

    EXPECT_TRUE(scanExecuted);
    EXPECT_EQ(foundDevices.size(), 1);
    EXPECT_TRUE(foundDevices.contains(std::make_pair(1, 0x50)));
    EXPECT_FALSE(foundDevices.contains(std::make_pair(1, 0x51)));
    EXPECT_FALSE(weak50.expired());
    EXPECT_TRUE(weak51.expired());
}

TEST_F(FruDeviceRescanTest, FailedBusScanPreservesExistingInterfaces)
{
    DBusIntfMap foundDevices;

    auto iface50 = createProbeInterface(1, 0x50, *objServer);
    auto iface51 = createProbeInterface(1, 0x51, *objServer);
    std::weak_ptr<sdbusplus::asio::dbus_interface> weak50 = iface50;
    std::weak_ptr<sdbusplus::asio::dbus_interface> weak51 = iface51;

    foundDevices.emplace(std::make_pair(1, 0x50), std::move(iface50));
    foundDevices.emplace(std::make_pair(1, 0x51), std::move(iface51));

    runRescan(foundDevices, *objServer,
              [&](ScanResult& result) { result.failedBuses.insert(1); });

    EXPECT_EQ(foundDevices.size(), 2);
    EXPECT_TRUE(foundDevices.contains(std::make_pair(1, 0x50)));
    EXPECT_TRUE(foundDevices.contains(std::make_pair(1, 0x51)));
    EXPECT_FALSE(weak50.expired());
    EXPECT_FALSE(weak51.expired());
}

TEST_F(FruDeviceRescanTest, RescanOneBusOnlyPrunesTargetBus)
{
    DBusIntfMap foundDevices;

    auto bus1Iface = createProbeInterface(1, 0x50, *objServer);
    auto bus2Iface = createProbeInterface(2, 0x50, *objServer);
    std::weak_ptr<sdbusplus::asio::dbus_interface> weakBus1 = bus1Iface;
    std::weak_ptr<sdbusplus::asio::dbus_interface> weakBus2 = bus2Iface;

    foundDevices.emplace(std::make_pair(1, 0x50), std::move(bus1Iface));
    foundDevices.emplace(std::make_pair(2, 0x50), std::move(bus2Iface));

    runRescan(
        foundDevices, *objServer,
        [&](ScanResult&) {
            EXPECT_FALSE(weakBus1.expired());
            EXPECT_FALSE(weakBus2.expired());
        },
        static_cast<size_t>(1));

    EXPECT_EQ(foundDevices.size(), 1);
    EXPECT_FALSE(foundDevices.contains(std::make_pair(1, 0x50)));
    EXPECT_TRUE(foundDevices.contains(std::make_pair(2, 0x50)));
    EXPECT_TRUE(weakBus1.expired());
    EXPECT_FALSE(weakBus2.expired());
}
