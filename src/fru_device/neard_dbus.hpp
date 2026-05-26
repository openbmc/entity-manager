#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>
#include <string>
#include <variant>
#include <vector>

constexpr const char* neardService = "org.neard";
constexpr const char* neardObjPath = "/org/neard/nfc0";
constexpr const char* neardRecordInterface = "org.neard.Record";
constexpr const char* propertyInterface = "org.freedesktop.DBus.Properties";
constexpr const char* introspectInterface =
    "org.freedesktop.DBus.Introspectable";

// Read org.neard.Record.MIMEPayload from a record object path
void getMimePayloadAsync(std::shared_ptr<sdbusplus::asio::connection> conn,
                         const std::string& objPath,
                         std::function<void(const std::vector<uint8_t>&)> payload);