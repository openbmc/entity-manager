#pragma once

#include "entity_manager/entity_manager.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <string>

using namespace std::string_literals;

static int randomSuffix()
{
    struct timespec ts{};
    clock_gettime(CLOCK_REALTIME, &ts);
    const unsigned int seed = ts.tv_nsec ^ getpid();
    srandom(seed);
    return random();
}

class TestEM
{
  public:
    TestEM(std::shared_ptr<sdbusplus::asio::connection>& systemBus,
           boost::asio::io_context& io) :
        io(io), uniqueSuffix(randomSuffix()),
        em(systemBus, io, {getTestConfigDir()}, getTestConfigDir()),
        busName(getRandomBusName())
    {
        systemBus->request_name(busName.c_str());
        lg2::debug("requesting bus name {NAME}", "NAME", busName);
    };

    ~TestEM()
    {
        std::filesystem::remove_all(getTestConfigDir());
    }

    void runStop()
    {
        // run through any init sequences at the end of a testcase.
        // This may involve async dbus calls, so run them here to avoid leaks
        for (size_t i = 0; i < 10; i++)
        {
            io.poll();
        }
    }

    std::string getRandomBusName() const
    {
        return "xyz.openbmc_project.EntityManager" +
               std::to_string(uniqueSuffix);
    }

    std::filesystem::path getTestConfigDir()
    {
        std::filesystem::path dir(std::format("/tmp/test_em_{}", uniqueSuffix));
        std::error_code ec;
        std::filesystem::create_directory(dir, ec);
        return dir;
    }

    boost::asio::io_context& io;
    const int uniqueSuffix;
    EntityManager em;

    const std::string busName;
};
