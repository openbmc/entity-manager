#include "entity_manager/entity_manager.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include "gtest/gtest.h"

using namespace std::string_literals;

static int myrandom()
{
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    const unsigned int seed = ts.tv_nsec ^ getpid();
    srandom(seed);
    return random();
}

static std::string getRandomBusName()
{
    return "xyz.openbmc_project.EntityManager" + std::to_string(myrandom());
}

// Helper class to access the protected members of the class we are testing
class TestEM : public EntityManager
{
  public:
    TestEM(std::shared_ptr<sdbusplus::asio::connection>& systemBus,
           boost::asio::io_context& io) : EntityManager(systemBus, io)
    {
        std::string busName = getRandomBusName();
        systemBus->request_name(busName.c_str());
    };

    // declare protected members of the base class as public here
    using EntityManager::dbusMatches;
    using EntityManager::interfacesAddedMatch;
    using EntityManager::interfacesRemovedMatch;
    using EntityManager::nameOwnerChangedMatch;
    using EntityManager::propertiesChangedInProgress;
    using EntityManager::propertiesChangedInstance;
    using EntityManager::propertiesChangedTimer;
    using EntityManager::scannedPowerOff;
    using EntityManager::scannedPowerOn;
};

// Test that EntityManager class can be constructed without exception
TEST(EntityManager, constructEM)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    TestEM em(systemBus, io);

    EXPECT_TRUE(em.nameOwnerChangedMatch == nullptr);
    EXPECT_TRUE(em.interfacesAddedMatch == nullptr);
    EXPECT_TRUE(em.interfacesRemovedMatch == nullptr);

    EXPECT_EQ(em.dbusMatches.size(), 0);
    EXPECT_EQ(em.propertiesChangedInstance, 0);
    EXPECT_EQ(em.scannedPowerOff, false);
    EXPECT_EQ(em.scannedPowerOn, false);
}

TEST(EntityManager, initFilters)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    TestEM em(systemBus, io);

    std::set<std::string> probeIntf = {"xyz.openbmc_project.Sensor.Value"};

    EXPECT_TRUE(em.nameOwnerChangedMatch == nullptr);
    EXPECT_TRUE(em.interfacesAddedMatch == nullptr);
    EXPECT_TRUE(em.interfacesRemovedMatch == nullptr);

    em.initFilters(probeIntf);

    EXPECT_TRUE(em.nameOwnerChangedMatch != nullptr);
    EXPECT_TRUE(em.interfacesAddedMatch != nullptr);
    EXPECT_TRUE(em.interfacesRemovedMatch != nullptr);

    // The above matches are not part of this map
    EXPECT_EQ(em.dbusMatches.size(), 0);
}

TEST(EntityManager, propertiesChangedCallback)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    TestEM em(systemBus, io);

    EXPECT_EQ(em.propertiesChangedInstance, 0);

    em.propertiesChangedCallback();

    EXPECT_EQ(em.propertiesChangedInstance, 1);
    EXPECT_FALSE(em.propertiesChangedInProgress);

    // timer should expire
    em.propertiesChangedTimer.wait();
}

TEST(EntityManager, registerCallback)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    TestEM em(systemBus, io);

    std::string path = "/xyz/openbmc_project/inventory/something";

    em.registerCallback(path);

    EXPECT_TRUE(em.dbusMatches.contains(path));
}

TEST(EntityManager, fwVersionIsSame)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    TestEM em(systemBus, io);

    // TODO: fix this to not throw or catch the exception internally
    EXPECT_THROW(em_utils::fwVersionIsSame(),
                 std::filesystem::filesystem_error);
}

TEST(EntityManager, handleCurrentConfigurationJson)
{
    boost::asio::io_context io;
    auto systemBus = std::make_shared<sdbusplus::asio::connection>(io);

    TestEM em(systemBus, io);

    // TODO: fix this to not throw or catch the exception internally
    EXPECT_THROW(em.handleCurrentConfigurationJson(),
                 std::filesystem::filesystem_error);
}
