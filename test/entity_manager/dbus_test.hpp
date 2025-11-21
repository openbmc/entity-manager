#pragma once

#include "test_em.hpp"
#include "utils.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include <gtest/gtest.h>

using namespace std::string_literals;

class DBusTest : public testing::Test
{
  protected:
    DBusTest() :
        systemBus(std::make_shared<sdbusplus::asio::connection>(io)),
        test(systemBus, io)
    {}
    ~DBusTest() noexcept override = default;

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    TestEM test;

  public:
    DBusTest(const DBusTest&) = delete;
    DBusTest(DBusTest&&) = delete;
    DBusTest& operator=(const DBusTest&) = delete;
    DBusTest& operator=(DBusTest&&) = delete;

    void postAssertHandlerCallback(
        const std::function<void(const DBusInterface& value)>& handler,
        bool stopOnError, const boost::system::error_code& ec,
        const DBusInterface& value)
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
        const std::function<void(const DBusInterface& value)>& handler,
        const std::string& path, const std::string& interface,
        bool stopOnError = true)
    {
        std::function<void(const boost::system::error_code&,
                           const DBusInterface&)>
            h3 = std::bind_front(&DBusTest::postAssertHandlerCallback, this,
                                 handler, stopOnError);

        systemBus->async_method_call(h3, test.busName, path,
                                     "org.freedesktop.DBus.Properties",
                                     "GetAll", interface);

        io.run();
    };
};
