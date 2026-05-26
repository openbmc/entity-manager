#include <sdbusplus/bus.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

constexpr std::string_view RECORD_TYPE_MIME = "MIME";
constexpr const char* NEARD_SERVICE = "org.neard";
constexpr const char* NEARD_OBJ_PATH = "/org/neard/nfc0";
constexpr const char* NEARD_RECORD_INTERFACE = "org.neard.Record";
constexpr const char* PROPERTY_INTERFACE = "org.freedesktop.DBus.Properties";
constexpr const char* INTROSPECT_INTERFACE =
    "org.freedesktop.DBus.Introspectable";

// Read org.neard.Record.MIMEPayload from a record object path
std::vector<uint8_t> getMimePayload(sdbusplus::bus_t& bus,
                                    const std::string& objPath);

// Find the NFC FRU record
std::optional<std::string> findFruRecordPath(sdbusplus::bus_t& bus);
