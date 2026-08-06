#pragma once

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>

#include <map>
#include <string>
#include <variant>
#include <vector>

constexpr const char* neardService = "org.neard";
constexpr const char* neardRecordInterface = "org.neard.Record";
constexpr const char* neardTagInterface = "org.neard.Tag";

using NeardPropsVariant =
    std::variant<std::string, bool, std::vector<std::string>,
                 std::vector<uint8_t>, sdbusplus::object_path>;
using NeardManagedObjectsType =
    std::map<sdbusplus::object_path,
             std::map<std::string, std::map<std::string, NeardPropsVariant>>>;

// Read org.neard.Record.MIMEPayload from a record object path
void getMimePayloadAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    const sdbusplus::object_path& objPath,
    const std::function<void(const std::vector<uint8_t>&)>& payload);

// Query neard for existing objects and invoke onMimeRecord for each MIME record
void queryNeardObjectsAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    const std::function<void(const sdbusplus::object_path&)>& onMimeRecord);
