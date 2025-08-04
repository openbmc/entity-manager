#pragma once

#include "test_em.hpp"

#include <nlohmann/json.hpp>

#include <flat_map>
#include <string>

#include <gtest/gtest.h>

using namespace std::string_literals;

using DBusPropertiesMap = std::flat_map<
    std::string,
    std::variant<std::string, int64_t, uint64_t, int32_t, uint32_t, double,
                 bool, std::vector<int64_t>, std::vector<uint64_t>,
                 std::vector<double>, std::vector<std::string>>>;

class DBusTest : public testing::Test
{
  protected:
    DBusTest() :
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        em(systemBus, io)
    {}
    ~DBusTest() noexcept override = default;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    TestEM em;

  public:
    DBusTest(const DBusTest&) = delete;
    DBusTest(DBusTest&&) = delete;
    DBusTest& operator=(const DBusTest&) = delete;
    DBusTest& operator=(DBusTest&&) = delete;

    void postAssertHandlerCallback(
        const std::function<void(const DBusPropertiesMap& value)>& handler,
        bool stopOnError, const boost::system::error_code& ec,
        const DBusPropertiesMap& value)
    {
        EXPECT_FALSE(ec);
        if (ec)
        {
            lg2::error("{ERROR}", "ERROR", ec.message());

            // not run below line to manually inspect DBus in case of test
            // failure
            if (stopOnError)
            {
                io.stop();
            }

            return;
        }

        try
        {
            handler(value);
            io.stop();
        }
        catch (std::exception& e)
        {
            if (stopOnError)
            {
                io.stop();
            }
        }
    };

    void postAssertHandler(
        const std::function<void(const DBusPropertiesMap& value)>& handler,
        const std::string& path, const std::string& interface,
        bool stopOnError = true)
    {
        std::function<void(const boost::system::error_code&,
                           const DBusPropertiesMap&)>
            h3 = std::bind_front(&DBusTest::postAssertHandlerCallback, this,
                                 handler, stopOnError);

        systemBus->async_method_call(h3, em.busName, path,
                                     "org.freedesktop.DBus.Properties",
                                     "GetAll", interface);

        io.run();
    };
};
