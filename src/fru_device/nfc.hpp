#pragma once

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/message/native_types.hpp>

#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>

struct FruDetails;

struct NfcUid
{
    explicit NfcUid(std::string uidIn) : uid(std::move(uidIn)) {}

    bool operator<(const NfcUid& other) const
    {
        return uid < other.uid;
    }

    bool operator==(const NfcUid& other) const
    {
        return uid == other.uid;
    }

    const std::string& str() const
    {
        return uid;
    }

  private:
    std::string uid;
};

struct NfcFruState
{
    std::map<sdbusplus::object_path, NfcUid> tagPathToUid;
    // maps NfcUid -> {i2c bus, i2c address}
    std::map<NfcUid, std::pair<uint32_t, uint32_t>> uidToFruKey;
    uint32_t nextAddress = 1;
};

// Use a reserved bus value for NFC-based FRU objects.
// These are not associated with any physical I2C device.
constexpr uint32_t nfcBus = std::numeric_limits<uint32_t>::max();

// Register NFC tag add/remove signal monitors and initialize already-present
// tags so FRU objects exist for both startup and runtime discovery.
void setupNfcMonitor(
    const std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    FruDetails& fruDetails,
    sdbusplus::asio::object_server& objServer,
    std::optional<sdbusplus::bus::match_t>& tagAddedMatch,
    std::optional<sdbusplus::bus::match_t>& tagRemovedMatch);

// Identify NFC-based FRU objects using the reserved bus.
// This is used to preserve them during I2C rescan.
inline bool isNfcFru(uint32_t bus)
{
    return bus == nfcBus;
}
