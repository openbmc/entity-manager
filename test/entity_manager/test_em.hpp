#pragma once

#include "entity_manager/entity_manager.hpp"
#include "phosphor-logging/lg2.hpp"

#include <nlohmann/json.hpp>

#include <string>

using namespace std::string_literals;

static int myrandom()
{
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    const unsigned int seed = ts.tv_nsec ^ getpid();
    srandom(seed);
    return random();
}

// Helper class to access the protected members of the class we are testing
class TestEM : public EntityManager
{
  public:
    TestEM(std::shared_ptr<sdbusplus::asio::connection>& systemBus,
           boost::asio::io_context& io) :
        EntityManager(systemBus, io, {getTestConfigDir()})
    {
        std::string busName = getRandomBusName();
        systemBus->request_name(busName.c_str());
        lg2::debug("requesting bus name {NAME}", "NAME", busName);
    };

    void runStop()
    {
        // run through any init sequences at the end of a testcase.
        // This may involve async dbus calls, so run them here to avoid leaks
        for (size_t i = 0; i < 10; i++)
        {
            io.poll();
        }
    }

    static std::string getRandomBusName()
    {
        return "xyz.openbmc_project.EntityManager" + std::to_string(myrandom());
    }

    static std::filesystem::path getTestConfigDir()
    {
        std::filesystem::path dir(std::format("/tmp/test_em_{}", myrandom()));
        std::error_code ec;
        std::filesystem::create_directory(dir, ec);
        return dir;
    }

    // declare protected members of the base class as public here
    using EntityManager::dbusMatches;
    using EntityManager::initFilters;
    using EntityManager::interfacesAddedMatch;
    using EntityManager::interfacesRemovedMatch;
    using EntityManager::nameOwnerChangedMatch;
    using EntityManager::propertiesChangedInProgress;
    using EntityManager::propertiesChangedInstance;
    using EntityManager::propertiesChangedTimer;
    using EntityManager::scannedPowerOff;
    using EntityManager::scannedPowerOn;
};
