#pragma once

#include "test_em.hpp"

#include <nlohmann/json.hpp>

#include <string>

#include <gtest/gtest.h>

using namespace std::string_literals;

using DBusPropertiesMap = boost::container::flat_map<
    std::string,
    std::variant<std::string, int64_t, double, bool, std::vector<int64_t>,
                 std::vector<uint64_t>, std::vector<std::string>>>;

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
        std::string& path, std::string& interface, bool stopOnError = true)
    {
        auto h3 = [this, handler, stopOnError](boost::system::error_code ec,
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

        boost::asio::post(io, [this, path, interface, h3]() {
            systemBus->async_method_call(h3, em.busName, path,
                                         "org.freedesktop.DBus.Properties",
                                         "GetAll", interface);
        });

        io.run();
    };
};
