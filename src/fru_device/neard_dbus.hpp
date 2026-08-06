#pragma once

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>

#include "../utils.hpp"

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

constexpr const char* neardService = "org.neard";
constexpr const char* neardRecordInterface = "org.neard.Record";
constexpr const char* neardTagInterface = "org.neard.Tag";

// Read org.neard.Record.MIMEPayload from a record object path
void getMimePayloadAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    const sdbusplus::object_path& objPath,
    std::function<void(const std::vector<uint8_t>&)> payload);

// Read org.neard.Tag.Uid from a tag object path
void getNfcTagUidAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    const sdbusplus::object_path& tagPath,
    std::function<void(const std::optional<std::string>&)> onUid);

// Query neard for existing objects and invoke onMimeRecord for each MIME record
void queryNeardObjectsAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    std::function<void(const sdbusplus::object_path&)> onMimeRecord);
