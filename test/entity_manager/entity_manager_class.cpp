#include "entity_manager/entity_manager.hpp"
#include "test_em.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include <gtest/gtest.h>

using namespace std::string_literals;

class TestEntityManager : public testing::Test
{
  public:
    TestEntityManager() :
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)) {};

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
};

// Test that EntityManager class can be constructed without exception
TEST_F(TestEntityManager, constructEM)
{
    TestEM em(systemBus, io);

    EXPECT_TRUE(em.nameOwnerChangedMatch != nullptr);
    EXPECT_TRUE(em.interfacesAddedMatch != nullptr);
    EXPECT_TRUE(em.interfacesRemovedMatch != nullptr);

    EXPECT_EQ(em.dbusMatches.size(), 0);
    EXPECT_EQ(em.propertiesChangedInstance, 0);
    EXPECT_EQ(em.scannedPowerOff, false);
    EXPECT_EQ(em.scannedPowerOn, false);

    em.runStop();
}

TEST_F(TestEntityManager, initFilters)
{
    TestEM em(systemBus, io);

    std::unordered_set<std::string> probeIntf = {
        "xyz.openbmc_project.Sensor.Value"};

    em.initFilters(probeIntf);

    EXPECT_TRUE(em.nameOwnerChangedMatch != nullptr);
    EXPECT_TRUE(em.interfacesAddedMatch != nullptr);
    EXPECT_TRUE(em.interfacesRemovedMatch != nullptr);

    // The above matches are not part of this map
    EXPECT_EQ(em.dbusMatches.size(), 0);

    em.runStop();
}

TEST_F(TestEntityManager, propertiesChangedCallback)
{
    TestEM em(systemBus, io);

    EXPECT_EQ(em.propertiesChangedInstance, 0);

    em.propertiesChangedCallback();

    EXPECT_EQ(em.propertiesChangedInstance, 1);
    EXPECT_FALSE(em.propertiesChangedInProgress);

    // timer should expire
    em.propertiesChangedTimer.wait();

    em.runStop();
}

TEST_F(TestEntityManager, registerCallback)
{
    TestEM em(systemBus, io);

    std::string path = "/xyz/openbmc_project/inventory/something";

    em.registerCallback(path);

    EXPECT_TRUE(em.dbusMatches.contains(path));

    em.runStop();
}

TEST_F(TestEntityManager, handleCurrentConfigurationJson)
{
    TestEM em(systemBus, io);

    em.handleCurrentConfigurationJson();

    em.runStop();
}
