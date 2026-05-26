#include "neard_dbus.hpp"

void getMimePayloadAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    const std::string& objPath,
    const std::function<void(const std::vector<uint8_t>&)>& payload)
{
    conn->async_method_call(
        [payload](const boost::system::error_code& ec,
                  const std::variant<std::vector<uint8_t>>& v) {
            if (ec)
            {
                lg2::error("Get MIMEPayload failed");
                return;
            }

            payload(std::get<std::vector<uint8_t>>(v));
        },
        neardService, objPath, propertyInterface, "Get", neardRecordInterface,
        "MIMEPayload");
}
