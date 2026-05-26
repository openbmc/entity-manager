#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/asio/connection.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

constexpr std::string_view recordTypeMime = "MIME";
constexpr const char* neardService = "org.neard";
constexpr const char* neardObjPath = "/org/neard/nfc0";
constexpr const char* neardRecordInterface = "org.neard.Record";
constexpr const char* propertyInterface = "org.freedesktop.DBus.Properties";
constexpr const char* introspectInterface =
    "org.freedesktop.DBus.Introspectable";

// Read org.neard.Record.MIMEPayload from a record object path
void getMimePayloadAsync(std::shared_ptr<sdbusplus::asio::connection> conn,
                         const std::string& objPath,
                         std::function<void(const std::vector<uint8_t>&)> cb);

// Find the NFC record
std::unique_ptr<sdbusplus::bus::match_t> startRecordMatch(std::shared_ptr<sdbusplus::asio::connection> conn,
                                                          std::function<void(const std::string&)> onRecordFound);


