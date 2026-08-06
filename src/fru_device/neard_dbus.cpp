#include "neard_dbus.hpp"

#include <iomanip>
#include <sstream>

static std::string uidToString(const std::vector<uint8_t>& uid)
{
    std::ostringstream uidStream;
    uidStream << std::hex << std::setfill('0');

    for (uint8_t b : uid)
    {
        uidStream << std::setw(2) << static_cast<unsigned int>(b);
    }

    return uidStream.str();
}

static void getMimePayloadCallback(
    const std::function<void(const std::vector<uint8_t>&)>& payload,
    const boost::system::error_code& ec,
    const std::map<std::string, DBusValueVariant>& props)
{
    if (ec)
    {
        lg2::error("Get MIMEPayload failed: {ERR}", "ERR", ec.message());
        return;
    }

    auto payloadIt = props.find("MIMEPayload");
    if (payloadIt == props.end())
    {
        lg2::error("Get MIMEPayload failed: MIMEPayload property missing");
        return;
    }

    const auto* mimePayload =
        std::get_if<std::vector<uint8_t>>(&payloadIt->second);
    if (mimePayload != nullptr)
    {
        payload(*mimePayload);
        return;
    }

    const auto* mimePayloadString =
        std::get_if<std::string>(&payloadIt->second);
    if (mimePayloadString != nullptr)
    {
        lg2::debug(
            "Get MIMEPayload decoded as string, converting to byte vector (size={SIZE})",
            "SIZE", mimePayloadString->size());
        std::vector<uint8_t> converted(mimePayloadString->begin(),
                                       mimePayloadString->end());
        payload(converted);
        return;
    }

    lg2::error("Get MIMEPayload failed: unexpected variant type index {IDX}",
               "IDX", payloadIt->second.index());
}

static void getTagUidCallback(
    const sdbusplus::object_path& tagPath,
    const std::function<void(const std::optional<std::string>&)>& onUid,
    const boost::system::error_code& ec,
    const std::map<std::string, DBusValueVariant>& props)
{
    if (ec)
    {
        lg2::error("Get Tag Uid failed: {ERR}", "ERR", ec.message());
        onUid(std::nullopt);
        return;
    }

    auto uidIt = props.find("Uid");
    if (uidIt == props.end())
    {
        lg2::error("Uid property missing for tag path {PATH}", "PATH", tagPath);
        onUid(std::nullopt);
        return;
    }

    const auto* uid = std::get_if<std::vector<uint8_t>>(&uidIt->second);
    if (uid == nullptr)
    {
        lg2::error("Uid property has unexpected type for tag path {PATH}",
                   "PATH", tagPath);
        onUid(std::nullopt);
        return;
    }

    std::string uidString = uidToString(*uid);
    lg2::debug("Detected NFC tag UID {UID} at {PATH}", "UID", uidString, "PATH",
               tagPath);
    onUid(uidString);
}

static void getManagedObjectsCallback(
    const std::function<void(const sdbusplus::object_path&)>& onMimeRecord,
    const boost::system::error_code& ec, const DBusManagedObjectsType& objects)
{
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

        const auto* type = std::get_if<std::string>(&typeIt->second);
        if (type == nullptr || *type != "MIME")
        {
            continue;
        }

        onMimeRecord(path);
    }
}

void getMimePayloadAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    const sdbusplus::object_path& objPath,
    std::function<void(const std::vector<uint8_t>&)> payload)
{
    std::function<void(const boost::system::error_code&,
                       const std::map<std::string, DBusValueVariant>&)>
        callback = std::bind_front(getMimePayloadCallback, std::move(payload));

    conn->async_method_call(std::move(callback), neardService, objPath,
                            "org.freedesktop.DBus.Properties", "GetAll",
                            neardRecordInterface);
}

void getNfcTagUidAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    const sdbusplus::object_path& tagPath,
    std::function<void(const std::optional<std::string>&)> onUid)
{
    std::function<void(const boost::system::error_code&,
                       const std::map<std::string, DBusValueVariant>&)>
        callback =
            std::bind_front(getTagUidCallback, tagPath, std::move(onUid));

    conn->async_method_call(std::move(callback), neardService, tagPath,
                            "org.freedesktop.DBus.Properties", "GetAll",
                            neardTagInterface);
}

void queryNeardObjectsAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    std::function<void(const sdbusplus::object_path&)> onMimeRecord)
{
    std::function<void(const boost::system::error_code&,
                       const DBusManagedObjectsType&)>
        callback =
            std::bind_front(getManagedObjectsCallback, std::move(onMimeRecord));

    conn->async_method_call(std::move(callback), neardService, "/",
                            "org.freedesktop.DBus.ObjectManager",
                            "GetManagedObjects");
}

std::optional<sdbusplus::object_path> getNeardTagPathFromRecordPath(
    const sdbusplus::object_path& recordPath)
{
    sdbusplus::object_path path = recordPath;
    while (path != "/" && !path.filename().starts_with("record"))
    {
        path = path.parent_path();
    }

    if (path == "/")
    {
        return std::nullopt;
    }

    return path.parent_path();
}
