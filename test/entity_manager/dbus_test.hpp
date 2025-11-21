#pragma once

#include "test_em.hpp"
#include "utils.hpp"

#include <boost/asio/steady_timer.hpp>
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

    static void postAssertHandlerCallback(
        boost::asio::io_context& io,
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
            lg2::debug("test: running assertion handler");
            handler(value);
            io.stop();
        }
        catch (std::exception& e)
        {
            lg2::error("caught exception: {ERR}", "ERR", e);
            if (stopOnError)
            {
                io.stop();
            }
        }
    };

    void postAssertHandler(

        const std::function<void(const DBusInterface& value)>& handler,
        const sdbusplus::message::object_path& path,
        const std::string& interface, bool stopOnError = true,
        const size_t timeoutMs = 1)
    {
        staticPostAssertHandler(io, systemBus, handler, test.busName, path,
                                interface, timeoutMs, stopOnError);
    }

    static void timerHandler(
        const std::shared_ptr<sdbusplus::asio::connection>& systemBus,
        const std::function<void(const boost::system::error_code&,
                                 const DBusInterface& value)>& handler,
        const std::string& busName, const sdbusplus::message::object_path& path,
        const std::string& interface,

        const boost::system::error_code& ec)
    {
        lg2::debug("timer expired");
        if (ec)
        {
            lg2::error("timer error");
            return;
        }
        systemBus->async_method_call(handler, busName, path,
                                     "org.freedesktop.DBus.Properties",
                                     "GetAll", interface);
    }

    static void staticPostAssertHandler(
        boost::asio::io_context& io,
        const std::shared_ptr<sdbusplus::asio::connection>& systemBus,
        const std::function<void(const DBusInterface& value)>& handler,
        const std::string& busName, const sdbusplus::message::object_path& path,
        const std::string& interface, const size_t timeoutMs,
        bool stopOnError = true)
    {
        lg2::debug("test: running assertion handler or {BUS} {PATH} {INTF}",
                   "BUS", busName, "PATH", path, "INTF", interface);

        std::function<void(const boost::system::error_code&,
                           const DBusInterface&)>
            h3 = std::bind_front(DBusTest::postAssertHandlerCallback,
                                 std::ref(io), handler, stopOnError);

        boost::asio::steady_timer timer(io);
        // we have to wait until properties changed is done

        lg2::debug("setting timer to {VALUE} ms", "VALUE", timeoutMs);
        timer.expires_after(std::chrono::milliseconds(timeoutMs));

        std::function<void(const boost::system::error_code&)> h4 =
            std::bind_front(timerHandler, systemBus, h3, busName, path,
                            interface);

        timer.async_wait(h4);

        io.run();
    };
};
