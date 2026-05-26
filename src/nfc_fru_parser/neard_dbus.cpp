#include "neard_dbus.hpp"

#include <sdbusplus/message.hpp>

#include <iostream>
#include <map>
#include <variant>

using Variant =
    std::variant<std::string, std::vector<uint8_t>, uint32_t, bool,
                 std::vector<std::string>, sdbusplus::message::object_path>;

void getMimePayloadAsync(std::shared_ptr<sdbusplus::asio::connection> conn,
                         const std::string& objPath,
                         std::function<void(const std::vector<uint8_t>&)> cb)
{
    conn->async_method_call(
    [cb](const boost::system::error_code& ec,
         const std::variant<std::vector<uint8_t>>& v)
    {
        if (ec)
        {
            std::cerr << "Get MIMEPayload failed: "
                      << ec.message() << "\n";
            cb({});
            return;
        }

        cb(std::get<std::vector<uint8_t>>(v));
    },
    neardService,
    objPath,
    propertyInterface,
    "Get",
    neardRecordInterface,
    "MIMEPayload");
}

std::unique_ptr<sdbusplus::bus::match_t> startRecordMatch(std::shared_ptr<sdbusplus::asio::connection> conn,
                                                          std::function<void(const std::string&)> recordFound)
{
    return std::make_unique<sdbusplus::bus::match_t>(
        *conn,
        "type='signal',"
        "sender='org.neard',"
        "interface='org.freedesktop.DBus.ObjectManager',"
        "member='InterfacesAdded'",
        [conn, recordFound](sdbusplus::message::message& msg) {

            std::cerr << "InterfacesAdded received\n";

            sdbusplus::message::object_path path;

            std::map<std::string,
                std::map<std::string,
                    std::variant<std::string, std::vector<uint8_t>>>>
                interfaces;

            try
            {
                msg.read(path, interfaces);
            }
            catch (...)
            {
                std::cerr << "Failed to parse InterfacesAdded\n";
                return;
            }

            auto it = interfaces.find("org.neard.Record");
            if (it == interfaces.end())
                return;

            const auto& props = it->second;

            auto typeIt = props.find("Type");
            if (typeIt == props.end())
                return;

            const auto* type =
                std::get_if<std::string>(&typeIt->second);

            if (*type != "MIME")
                return;

            recordFound(std::string(path));
        });
}
