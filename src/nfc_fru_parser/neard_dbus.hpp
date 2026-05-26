#include <sdbusplus/bus.hpp>

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
std::vector<uint8_t> getMimePayload(sdbusplus::bus_t& bus,
                                    const std::string& objPath);

// Find the NFC FRU record
std::optional<std::string> findFruRecordPath(sdbusplus::bus_t& bus);
