#include "neard_dbus.hpp"

void getMimePayloadAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    const sdbusplus::object_path& objPath,
    const std::function<void(const std::vector<uint8_t>&)>& payload)
{
    conn->async_method_call(
        [payload](const boost::system::error_code& ec,
                  const std::variant<std::vector<uint8_t>>& v) {
            if (ec)
            {
                lg2::error("Get MIMEPayload failed: {ERR}", "ERR",
                           ec.message());
                return;
            }

            payload(std::get<std::vector<uint8_t>>(v));
        },
        neardService, objPath, "org.freedesktop.DBus.Properties", "Get",
        neardRecordInterface, "MIMEPayload");
}

void queryNeardObjectsAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    const std::function<void(const sdbusplus::object_path&)>& onMimeRecord)
{
    conn->async_method_call(
        [onMimeRecord](const boost::system::error_code& ec,
                       const NeardManagedObjectsType& objects) {
            if (ec)
            {
                lg2::debug("neard GetManagedObjects failed: {ERR}", "ERR",
                           ec.message());
                return;
            }

            for (const auto& [path, interfaces] : objects)
            {
                auto ifaceIt = interfaces.find(neardRecordInterface);
                if (ifaceIt == interfaces.end())
                {
                    continue;
                }

                const auto& props = ifaceIt->second;
                auto typeIt = props.find("Type");
                if (typeIt == props.end())
                {
                    continue;
                }

                const auto* type =
                    std::get_if<std::string>(&typeIt->second);
                if (type == nullptr || *type != "MIME")
                {
                    continue;
                }

                onMimeRecord(path);
            }
        },
        neardService, "/", "org.freedesktop.DBus.ObjectManager",
        "GetManagedObjects");
}
