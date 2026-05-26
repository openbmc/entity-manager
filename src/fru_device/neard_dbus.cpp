#include "neard_dbus.hpp"

void getMimePayloadAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    const sdbusplus::object_path& objPath,
    const std::function<void(const std::vector<uint8_t>&)>& onMimePayload)
{
    conn->async_method_call(
        [onMimePayload](const boost::system::error_code& ec,
                  const std::variant<std::vector<uint8_t>>& v) {
            if (ec)
            {
                lg2::error("Get MIMEPayload failed");
                return;
            }

            onMimePayload(std::get<std::vector<uint8_t>>(v));
        },
        neardService, objPath, "org.freedesktop.DBus.Properties", "Get",
        neardRecordInterface, "MIMEPayload");
}
