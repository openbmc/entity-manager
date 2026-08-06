#include "nfc.hpp"

#include "fru_device.hpp"
#include "neard_dbus.hpp"

#include <algorithm>

static void handleNfcPayload(
    std::vector<uint8_t>& payload, FruDetails& fruDetails,
    const sdbusplus::object_path& tagPath,
    const NfcUid& uid, sdbusplus::asio::object_server& objServer)
{
    if (payload.empty())
    {
        lg2::warning("Empty payload");
        return;
    }

    NfcFruState& nfcFruState = fruDetails.nfcFruState;
    auto existing = nfcFruState.uidToFruKey.find(uid);
    if (existing != nfcFruState.uidToFruKey.end())
    {
        auto ifaceIt = fruDetails.dbusInterfaceMap.find(existing->second);
        if (ifaceIt != fruDetails.dbusInterfaceMap.end())
        {
            objServer.remove_interface(ifaceIt->second);
            fruDetails.dbusInterfaceMap.erase(ifaceIt);
        }
    }

    for (auto tagIt = nfcFruState.tagPathToUid.begin();
         tagIt != nfcFruState.tagPathToUid.end();)
    {
        if (tagIt->second == uid && tagIt->first != tagPath)
        {
            tagIt = nfcFruState.tagPathToUid.erase(tagIt);
        }
        else
        {
            ++tagIt;
        }
    }

    const std::pair<uint32_t, uint32_t> nfcKey{nfcBus,
                                               nfcFruState.nextAddress++};
    addFruObjectToDbus(payload, fruDetails, nfcBus, nfcKey.second, objServer);

    auto newIface = fruDetails.dbusInterfaceMap.find(nfcKey);
    if (newIface == fruDetails.dbusInterfaceMap.end())
    {
        lg2::error("Failed to create NFC FRU object for UID {UID}", "UID",
                   uid.str());
        return;
    }

    nfcFruState.tagPathToUid.insert_or_assign(tagPath, uid);
    nfcFruState.uidToFruKey.insert_or_assign(uid, nfcKey);
    lg2::debug("NFC FRU object created for UID {UID}", "UID", uid.str());
}

static void handleNfcTagAdded(
    const std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    FruDetails& fruDetails,
    sdbusplus::asio::object_server& objServer, sdbusplus::message_t& message)
{
    sdbusplus::object_path path;

    std::map<std::string, std::map<std::string, DBusValueVariant>> interfaces;

    message.read(path, interfaces);

    auto it = interfaces.find(neardRecordInterface);
    if (it == interfaces.end())
    {
        return;
    }

    const auto& props = it->second;

    auto typeIt = props.find("Type");
    if (typeIt == props.end())
    {
        return;
    }

    const auto* type = std::get_if<std::string>(&typeIt->second);
    if (type == nullptr)
    {
        lg2::error("Invalid Type property value");
        return;
    }

    if (*type != "MIME")
    {
        lg2::debug("Ignoring non-MIME record, Type={TYPE}", "TYPE", *type);
        return;
    }

    auto tagPath = getNeardTagPathFromRecordPath(path);
    if (!tagPath)
    {
        lg2::error("Cannot derive tag path from record path: {PATH}", "PATH",
                   static_cast<const std::string&>(path));
        return;
    }

    getNfcTagUidAsync(
        systemBus, *tagPath,
        [systemBus, path, tagPath = *tagPath, &fruDetails,
         &objServer](const std::optional<std::string>& uid) {
            if (!uid)
            {
                return;
            }

            NfcUid nfcUid(*uid);

            getMimePayloadAsync(
                systemBus, path,
                [tagPath, nfcUid = std::move(nfcUid), &fruDetails,
                 &objServer](std::vector<uint8_t> payload) {
                    handleNfcPayload(payload, fruDetails, tagPath, nfcUid,
                                     objServer);
                });
        });
}

static void handleNfcTagRemoved(FruDetails& fruDetails,
                                sdbusplus::asio::object_server& objServer,
                                sdbusplus::message_t& message)
{
    sdbusplus::object_path path;
    std::vector<std::string> interfaces;

    message.read(path, interfaces);

    auto it =
        std::find(interfaces.begin(), interfaces.end(), neardTagInterface);
    if (it == interfaces.end())
    {
        return;
    }

    NfcFruState& nfcFruState = fruDetails.nfcFruState;
    auto uidIt = nfcFruState.tagPathToUid.find(path);
    if (uidIt == nfcFruState.tagPathToUid.end())
    {
        return;
    }

    auto fruKeyIt = nfcFruState.uidToFruKey.find(uidIt->second);
    if (fruKeyIt == nfcFruState.uidToFruKey.end())
    {
        nfcFruState.tagPathToUid.erase(uidIt);
        return;
    }

    auto ifaceIt = fruDetails.dbusInterfaceMap.find(fruKeyIt->second);
    if (ifaceIt != fruDetails.dbusInterfaceMap.end())
    {
        objServer.remove_interface(ifaceIt->second);
        fruDetails.dbusInterfaceMap.erase(ifaceIt);
        lg2::info(
            "NFC FRU object removed for UID {UID}, key bus={BUS}, addr={ADDR}",
            "UID", uidIt->second.str(), "BUS", fruKeyIt->second.first, "ADDR",
            fruKeyIt->second.second);
    }

    nfcFruState.uidToFruKey.erase(fruKeyIt);
    nfcFruState.tagPathToUid.erase(uidIt);
}

static void queryNeardObjects(
    const std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    FruDetails& fruDetails,
    sdbusplus::asio::object_server& objServer)
{
    queryNeardObjectsAsync(
        systemBus, [systemBus, &fruDetails,
                    &objServer](const sdbusplus::object_path& path) {
            auto tagPath = getNeardTagPathFromRecordPath(path);
            if (!tagPath)
            {
                return;
            }

            getNfcTagUidAsync(
                systemBus, *tagPath,
                [systemBus, path, tagPath = *tagPath, &fruDetails,
                 &objServer](const std::optional<std::string>& uid) {
                    if (!uid)
                    {
                        return;
                    }

                    NfcUid nfcUid(*uid);

                    getMimePayloadAsync(
                        systemBus, path,
                        [tagPath, nfcUid = std::move(nfcUid), &fruDetails,
                         &objServer](std::vector<uint8_t> payload) {
                            handleNfcPayload(payload, fruDetails, tagPath,
                                             nfcUid, objServer);
                        });
                });
        });
}

void setupNfcMonitor(
    const std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    FruDetails& fruDetails,
    sdbusplus::asio::object_server& objServer,
    std::optional<sdbusplus::bus::match_t>& tagAddedMatch,
    std::optional<sdbusplus::bus::match_t>& tagRemovedMatch)
{
    using namespace sdbusplus::bus::match::rules;

    auto nfcTagAddedHandler = [systemBus, &fruDetails,
                               &objServer](sdbusplus::message_t& message) {
        handleNfcTagAdded(systemBus, fruDetails, objServer, message);
    };

    auto nfcTagRemovedHandler =
        [&fruDetails, &objServer](sdbusplus::message_t& message) {
            handleNfcTagRemoved(fruDetails, objServer, message);
        };

    tagAddedMatch.emplace(static_cast<sdbusplus::bus_t&>(*systemBus),
                          interfacesAdded() + sender("org.neard"),
                          nfcTagAddedHandler);

    tagRemovedMatch.emplace(static_cast<sdbusplus::bus_t&>(*systemBus),
                            interfacesRemoved() + sender("org.neard"),
                            nfcTagRemovedHandler);

    // Query neard for NFC tags already present at startup.
    queryNeardObjects(systemBus, fruDetails, objServer);
}
