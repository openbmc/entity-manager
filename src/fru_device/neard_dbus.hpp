#pragma once

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message.hpp>

#include <string>
#include <variant>
#include <vector>

constexpr const char* neardService = "org.neard";
constexpr const char* neardRecordInterface = "org.neard.Record";

// Read org.neard.Record.MIMEPayload from a record object path
void getMimePayloadAsync(
    const std::shared_ptr<sdbusplus::asio::connection>& conn,
    const sdbusplus::object_path& objPath,
    const std::function<void(const std::vector<uint8_t>&)>& payload);
