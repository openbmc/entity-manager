#pragma once

#include "entity_manager/entity_manager.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <chrono>
#include <fstream>
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

class TestConfigDir
{
  public:
    TestConfigDir();
    explicit TestConfigDir(nlohmann::json& testConfig) :
        uniqueSuffix(randomSuffix())
    {
        const std::filesystem::path configDir = getTestConfigDir();

        const std::filesystem::path configPath =
            configDir / "example_board.json";

        lg2::debug("writing test configuration file to {PATH}", "PATH",
                   configPath);

        std::ofstream configFile(configPath);
        configFile << testConfig.dump();
        configFile.close();
    }

    std::filesystem::path getTestConfigDir()
    {
        std::filesystem::path dir(std::format("/tmp/test_em_{}", uniqueSuffix));
        std::error_code ec;
        std::filesystem::create_directory(dir, ec);
        return dir;
    }

    const int uniqueSuffix;

    ~TestConfigDir()
    {
        std::filesystem::remove_all(getTestConfigDir());
    }
};

// Helper class to access the protected members of the class we are testing
class TestEM : public TestConfigDir
{
  public:
    TestEM(std::shared_ptr<sdbusplus::asio::connection>& systemBus,
           boost::asio::io_context& io, nlohmann::json testConfig = {}) :
        TestConfigDir(testConfig), io(io),
        em(systemBus, io, {getTestConfigDir()}, "schemas/", false,
           getTestConfigDir(), std::chrono::milliseconds(1),
           std::chrono::milliseconds(1), std::chrono::milliseconds(1),
           std::chrono::milliseconds(1), std::chrono::milliseconds(1)),
        busName(getRandomBusName())
    {
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

    std::string getRandomBusName() const
    {
        return "xyz.openbmc_project.EntityManager" +
               std::to_string(uniqueSuffix);
    }

    boost::asio::io_context& io;
    EntityManager em;

    const std::string busName;
};
