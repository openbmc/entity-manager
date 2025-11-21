#pragma once

#include "test_em.hpp"

#include <boost/asio/steady_timer.hpp>
#include <nlohmann/json.hpp>

#include <string>

#include <gtest/gtest.h>

using namespace std::string_literals;

using DBusPropertiesMap = boost::container::flat_map<
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
    nlohmann::json systemConfiguration = "{}";
    std::string boardNameOrig = "myboard";
    std::string jsonPointerPath = "xyz";

    std::string configInterface = "xyz.openbmc_project.Configuration.Example";
    std::string objPath =
        "/xyz/openbmc_project/inventory/system/board/MyBoard/MyRecord";

    // This is written in the testcase
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;

    // The configuration to expose. Gets written in the testcase
    nlohmann::json dict;

  public:
    DBusTest(const DBusTest&) = delete;
    DBusTest(DBusTest&&) = delete;
    DBusTest& operator=(const DBusTest&) = delete;
    DBusTest& operator=(DBusTest&&) = delete;

    void postAssertHandler(
        const std::function<void(const DBusPropertiesMap& value)>& handler,
        const std::string& busName, const sdbusplus::message::object_path& path,
        const std::string& interface, bool stopOnError = true,
        const size_t timeoutMs = 1)
    {
        staticPostAssertHandler(io, systemBus, handler, busName, path,
                                interface, timeoutMs, stopOnError);
    }

    static void staticPostAssertHandler(
        boost::asio::io_context& io,
        const std::shared_ptr<sdbusplus::asio::connection>& systemBus,
        const std::function<void(const DBusPropertiesMap& value)>& handler,
        const std::string& busName, const sdbusplus::message::object_path& path,
        const std::string& interface, const size_t timeoutMs,
        bool stopOnError = true)
    {
        lg2::debug("test: running assertion handler or {BUS} {PATH} {INTF}",
                   "BUS", busName, "PATH", path, "INTF", interface);

        auto h3 = [&io, handler, stopOnError](boost::system::error_code ec,
                                              const DBusPropertiesMap& value) {
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

        boost::asio::steady_timer timer(io);
        // we have to wait until properties changed is done

        lg2::debug("setting timer to {VALUE} ms", "VALUE", timeoutMs);
        timer.expires_after(std::chrono::milliseconds(timeoutMs));

        boost::asio::post(io, [systemBus, busName, path, interface, &h3,
                               &timer]() {
            timer.async_wait([&h3, busName, path, interface,
                              systemBus](const boost::system::error_code& ec) {
                lg2::debug("timer expired");
                if (ec)
                {
                    lg2::error("timer error");
                    return;
                }
                systemBus->async_method_call(h3, busName, path,
                                             "org.freedesktop.DBus.Properties",
                                             "GetAll", interface);
            });
        });

        io.run();
    };
};
