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
    TestEM test(systemBus, io);

    test.runStop();
}

TEST_F(TestEntityManager, propertiesChangedCallback)
{
    TestEM test(systemBus, io);

    EXPECT_EQ(test.em.getPropertiesChangedInstance(), 0);

    test.em.propertiesChangedCallback();

    EXPECT_EQ(test.em.getPropertiesChangedInstance(), 1);
    EXPECT_FALSE(test.em.isPropertiesChangedInProgress());

    test.runStop();
}

TEST_F(TestEntityManager, registerCallback)
{
    TestEM test(systemBus, io);

    std::string path = "/xyz/openbmc_project/inventory/something";

    test.em.registerCallback(path);

    EXPECT_TRUE(test.em.hasDBusMatch(path));

    test.runStop();
}

TEST_F(TestEntityManager, handleCurrentConfigurationJson)
{
    TestEM test(systemBus, io);

    test.em.handleCurrentConfigurationJson();

    test.runStop();
}
