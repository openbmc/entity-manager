// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "perform_scan.hpp"

#include "perform_probe.hpp"
#include "utils.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>

#include <cerrno>
#include <charconv>
#include <flat_map>
#include <flat_set>

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

constexpr const int32_t maxMapperDepth = 0;

struct DBusInterfaceInstance
{
    std::string busName;
    std::string path;
    std::string interface;
};

void getInterfaces(
    const DBusInterfaceInstance& instance,
    const std::vector<std::shared_ptr<probe::PerformProbe>>& probeVector,
    const std::shared_ptr<scan::PerformScan>& scan, boost::asio::io_context& io,
    size_t retries = 5)
{
    if (retries == 0U)
    {
        lg2::error("retries exhausted on {BUSNAME} {PATH} {INTF}", "BUSNAME",
                   instance.busName, "PATH", instance.path, "INTF",
                   instance.interface);
        return;
    }

    scan->_em.systemBus->async_method_call(
        [instance, scan, probeVector, retries,
         &io](boost::system::error_code& errc,
              const DBusInterface& resp) mutable {
            if (errc)
            {
                // EBADR indicates the D-Bus object was removed between
                // GetSubTree and GetAll. This corresponds to
                // org.freedesktop.DBus.Error.UnknownObject and is expected
                // during concurrent device removal. Skip retry to avoid
                // unnecessary delays.
                if (errc.value() == EBADR)
                {
                    lg2::info("D-Bus object removed during scan, skipping: "
                              "{BUSNAME} {PATH} {INTF}",
                              "BUSNAME", instance.busName, "PATH",
                              instance.path, "INTF", instance.interface);
                    return;
                }

                lg2::error("error calling getall on {BUSNAME} {PATH} {INTF}",
                           "BUSNAME", instance.busName, "PATH", instance.path,
                           "INTF", instance.interface);

                auto timer = std::make_shared<boost::asio::steady_timer>(io);
                timer->expires_after(std::chrono::seconds(2));

                timer->async_wait([timer, instance, scan, probeVector, retries,
                                   &io](const boost::system::error_code&) {
                    getInterfaces(instance, probeVector, scan, io, retries - 1);
                });
                return;
            }

            scan->dbusProbeObjects[std::string(instance.path)]
                                  [std::string(instance.interface)] = resp;
        },
        instance.busName, instance.path, "org.freedesktop.DBus.Properties",
        "GetAll", instance.interface);
}

static void processDbusObjects(
    std::vector<std::shared_ptr<probe::PerformProbe>>& probeVector,
    const std::shared_ptr<scan::PerformScan>& scan,
    const GetSubTreeType& interfaceSubtree, boost::asio::io_context& io)
{
    for (const auto& [path, object] : interfaceSubtree)
    {
        // Get a PropertiesChanged callback for all interfaces on this path.
        scan->_em.registerCallback(path);

        for (const auto& [busname, ifaces] : object)
        {
            for (const std::string& iface : ifaces)
            {
                // The 3 default org.freedeskstop interfaces (Peer,
                // Introspectable, and Properties) are returned by
                // the mapper but don't have properties, so don't bother
                // with the GetAll call to save some cycles.
                if (!iface.starts_with("org.freedesktop"))
                {
                    getInterfaces({busname, path, iface}, probeVector, scan,
                                  io);
                }
            }
        }
    }
}

// Populates scan->dbusProbeObjects with all interfaces and properties
// for the paths that own the interfaces passed in.
void findDbusObjects(
    std::vector<std::shared_ptr<probe::PerformProbe>>&& probeVector,
    std::flat_set<std::string, std::less<>>&& interfaces,
    const std::shared_ptr<scan::PerformScan>& scan, boost::asio::io_context& io,
    size_t retries = 5)
{
    // Filter out interfaces already obtained.
    for (const auto& [path, probeInterfaces] : scan->dbusProbeObjects)
    {
        for (const auto& [interface, _] : probeInterfaces)
        {
            interfaces.erase(interface);
        }
    }
    if (interfaces.empty())
    {
        return;
    }

    // find all connections in the mapper that expose a specific type
    scan->_em.systemBus->async_method_call(
        [interfaces, probeVector{std::move(probeVector)}, scan, retries,
         &io](boost::system::error_code& ec,
              const GetSubTreeType& interfaceSubtree) mutable {
            if (ec)
            {
                if (ec.value() == ENOENT)
                {
                    return; // wasn't found by mapper
                }
                lg2::error("Error communicating to mapper.");

                if (retries == 0U)
                {
                    // if we can't communicate to the mapper something is very
                    // wrong
                    std::exit(EXIT_FAILURE);
                }

                auto timer = std::make_shared<boost::asio::steady_timer>(io);
                timer->expires_after(std::chrono::seconds(10));

                timer->async_wait(
                    [timer, interfaces{std::move(interfaces)}, scan,
                     probeVector{std::move(probeVector)}, retries,
                     &io](const boost::system::error_code&) mutable {
                        findDbusObjects(std::move(probeVector),
                                        std::move(interfaces), scan, io,
                                        retries - 1);
                    });
                return;
            }

            processDbusObjects(probeVector, scan, interfaceSubtree, io);
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree", "/", maxMapperDepth,
        interfaces);
}

static std::string getRecordName(const DBusInterface& probe,
                                 const std::string& probeName)
{
    if (probe.empty())
    {
        return probeName;
    }

    // use an array so alphabetical order from the flat_map is maintained
    auto device = nlohmann::json::array();
    for (const auto& devPair : probe)
    {
        device.push_back(devPair.first);
        std::visit([&device](auto&& v) { device.push_back(v); },
                   devPair.second);
    }

    // hashes are hard to distinguish, use the non-hashed version if we want
    // debug
    // return probeName + device.dump();

    return std::to_string(std::hash<std::string>{}(probeName + device.dump()));
}

scan::PerformScan::PerformScan(
    EntityManager& em, SystemConfiguration& missingConfigurations,
    std::vector<EMConfig>& configurations, boost::asio::io_context& io,
    std::function<void()>&& callback) :
    _em(em), _missingConfigurations(missingConfigurations),
    _configurations(configurations), _callback(std::move(callback)), io(io)
{}

static void recordDiscoveredIdentifiers(
    std::set<nlohmann::json>& usedNames, std::list<size_t>& indexes,
    const std::string& probeName, const EMConfig& record)
{
    size_t indexIdx = probeName.find('$');
    if (indexIdx == std::string::npos)
    {
        return;
    }

    int index = 0;
    auto str = record.name.substr(indexIdx);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const char* endPtr = str.data() + str.size();
    auto [p, ec] = std::from_chars(str.data(), endPtr, index);
    if (ec != std::errc())
    {
        return; // non-numeric replacement
    }

    usedNames.insert(record.name);

    auto usedIt = std::find(indexes.begin(), indexes.end(), index);
    if (usedIt != indexes.end())
    {
        indexes.erase(usedIt);
    }
}

static bool extractExposeActionRecordNames(std::vector<std::string>& matches,
                                           const std::string& exposeKey,
                                           const nlohmann::json& exposeValue)
{
    const std::string* exposeValueStr =
        exposeValue.get_ptr<const std::string*>();
    if (exposeValueStr != nullptr)
    {
        matches.emplace_back(*exposeValueStr);
        return true;
    }

    const nlohmann::json::array_t* exarr =
        exposeValue.get_ptr<const nlohmann::json::array_t*>();
    if (exarr != nullptr)
    {
        for (const auto& value : *exarr)
        {
            if (!value.is_string())
            {
                lg2::error("Value is invalid type {VALUE}", "VALUE",
                           value.dump());
                break;
            }
            matches.emplace_back(value);
        }

        return true;
    }

    lg2::error("Value is invalid type {KEY}", "KEY", exposeKey);
    return false;
}

static std::optional<std::vector<std::string>::iterator> findExposeActionRecord(
    std::vector<std::string>& matches, const nlohmann::json& record)
{
    const auto& name = (record)["Name"].get_ref<const std::string&>();
    auto compare = [&name](const std::string& s) { return s == name; };
    auto matchIt = std::find_if(matches.begin(), matches.end(), compare);

    if (matchIt == matches.end())
    {
        return std::nullopt;
    }

    return matchIt;
}

static void applyBindExposeAction(nlohmann::json::object_t& exposedObject,
                                  nlohmann::json::object_t& expose,
                                  const std::string& propertyName)
{
    if (propertyName.starts_with("Bind"))
    {
        std::string bind = propertyName.substr(sizeof("Bind") - 1);
        exposedObject["Status"] = "okay";
        expose[bind] = exposedObject;
    }
}

static void applyDisableExposeAction(nlohmann::json::object_t& exposedObject,
                                     const std::string& propertyName)
{
    if (propertyName == "DisableNode")
    {
        exposedObject["Status"] = "disabled";
    }
}

static void applyConfigExposeActions(
    std::vector<std::string>& matches, nlohmann::json::object_t& expose,
    const std::string& propertyName, EMConfig& config)
{
    for (auto& exposedObject : config.exposesRecords)
    {
        auto match = findExposeActionRecord(matches, exposedObject);
        if (match)
        {
            matches.erase(*match);

            nlohmann::json::object_t* exposedObjectObj = &exposedObject;

            if (exposedObjectObj == nullptr)
            {
                lg2::error("Exposed object wasn't a object: {JSON}", "JSON",
                           nlohmann::json(exposedObject).dump());
                continue;
            }

            applyBindExposeAction(*exposedObjectObj, expose, propertyName);
            applyDisableExposeAction(*exposedObjectObj, propertyName);
        }
    }
}

static void applyExposeActions(
    SystemConfiguration& systemConfiguration, const std::string& recordName,
    nlohmann::json::object_t& expose, const std::string& exposeKey,
    nlohmann::json::object_t& exposeValue)
{
    bool isBind = exposeKey.starts_with("Bind");
    bool isDisable = exposeKey == "DisableNode";
    bool isExposeAction = isBind || isDisable;

    if (!isExposeAction)
    {
        return;
    }

    std::vector<std::string> matches;

    if (!extractExposeActionRecordNames(matches, exposeKey,
                                        nlohmann::json(exposeValue)))
    {
        return;
    }

    for (const auto& [configId, config] : systemConfiguration)
    {
        // don't disable ourselves
        if (isDisable && configId == recordName)
        {
            continue;
        }

        applyConfigExposeActions(matches, expose, exposeKey, config);
    }

    if (!matches.empty())
    {
        lg2::error(
            "configuration file dependency error, could not find {KEY} {VALUE}",
            "KEY", exposeKey, "VALUE", nlohmann::json(exposeValue).dump());
    }
}

static std::string generateDeviceName(
    const std::set<nlohmann::json>& usedNames, const DBusObject& dbusObject,
    size_t foundDeviceIdx, const std::string& nameTemplate,
    std::optional<std::string>& replaceStr)
{
    std::string copyForName = nameTemplate;
    std::optional<std::string> replaceVal = em_utils::templateCharReplace(
        copyForName, dbusObject, foundDeviceIdx, replaceStr);

    if (!replaceStr && replaceVal)
    {
        if (usedNames.contains(nameTemplate))
        {
            replaceStr = replaceVal;
            em_utils::templateCharReplace(copyForName, dbusObject,
                                          foundDeviceIdx, replaceStr);
        }
    }

    if (replaceStr)
    {
        lg2::error(
            "Duplicates found, replacing {STR} with found device index. Consider fixing template to not have duplicates",
            "STR", *replaceStr);
    }
    const std::string* ret = &copyForName;
    if (ret == nullptr)
    {
        lg2::error("Device name wasn't a string: ${JSON}", "JSON", copyForName);
        return "";
    }
    return *ret;
}
static void applyTemplateAndExposeActions(
    const std::string& recordName, const DBusObject& dbusObject,
    size_t foundDeviceIdx, const std::optional<std::string>& replaceStr,
    nlohmann::json::object_t& value, SystemConfiguration& systemConfiguration)
{
    // we need to convert into this type to avoid ambiguous overloads
    // with templateCharReplace.
    nlohmann::json json = value;

    for (auto& [key, valueInner] : value)
    {
        em_utils::templateCharReplace(json, dbusObject, foundDeviceIdx,
                                      replaceStr);
        applyExposeActions(systemConfiguration, recordName, value, key, value);
    }

    // write back
    if (json.type() == nlohmann::json::value_t::object)
    {
        value = *(json.get_ptr<const nlohmann::json::object_t*>());
    }
};

void scan::PerformScan::updateSystemConfiguration(const EMConfig& recordRef,
                                                  const std::string& probeName,
                                                  FoundDevices& foundDevices)
{
    _passed = true;
    passedProbes.push_back(probeName);

    std::set<nlohmann::json> usedNames;
    std::list<size_t> indexes(foundDevices.size());
    std::iota(indexes.begin(), indexes.end(), 1);

    // copy over persisted configurations and make sure we remove
    // indexes that are already used
    for (auto itr = foundDevices.begin(); itr != foundDevices.end();)
    {
        std::string recordName = getRecordName(itr->interface, probeName);

        EMConfig* record = nullptr;

        if (!_em.systemConfiguration.contains(recordName))
        {
            if (!_em.lastJson.contains(recordName))
            {
                itr++;
                continue;
            }

            record = &_em.lastJson.at(recordName);
        }
        else
        {
            record = &_em.systemConfiguration[recordName];
        }

        _missingConfigurations.erase(recordName);

        // We've processed the device, remove it and advance the
        // iterator
        itr = foundDevices.erase(itr);
        recordDiscoveredIdentifiers(usedNames, indexes, probeName, *record);
    }

    std::optional<std::string> replaceStr;

    DBusObject emptyObject;
    DBusInterface emptyInterface;
    emptyObject.emplace(std::string{}, emptyInterface);

    for (const auto& [foundDevice, path] : foundDevices)
    {
        // Need all interfaces on this path so that template
        // substitutions can be done with any of the contained
        // properties.  If the probe that passed didn't use an
        // interface, such as if it was just TRUE, then
        // templateCharReplace will just get passed in an empty
        // map.
        auto objectIt = dbusProbeObjects.find(path);
        const DBusObject& dbusObject = (objectIt == dbusProbeObjects.end())
                                           ? emptyObject
                                           : objectIt->second;

        // we make a copy here to modify
        EMConfig record = recordRef;

        const std::string recordName = getRecordName(foundDevice, probeName);
        size_t foundDeviceIdx = indexes.front();
        indexes.pop_front();

        std::string deviceName = generateDeviceName(
            usedNames, dbusObject, foundDeviceIdx, recordRef.name, replaceStr);

        record.name = deviceName;

        usedNames.insert(deviceName);

        em_utils::templateCharReplace(record.type, dbusObject, foundDeviceIdx,
                                      replaceStr, true);

        for (auto& probe : record.probeStmt)
        {
            em_utils::templateCharReplace(probe, dbusObject, foundDeviceIdx,
                                          replaceStr, false);
        }

        // insert into configuration temporarily to be able to
        // reference ourselves

        _em.systemConfiguration.insert_or_assign(recordName, record);

        for (auto& value : record.exposesRecords)
        {
            applyTemplateAndExposeActions(recordName, dbusObject,
                                          foundDeviceIdx, replaceStr, value,
                                          _em.systemConfiguration);
        }

        for (const auto& key : record.extraInterfaces.keys())
        {
            applyTemplateAndExposeActions(
                recordName, dbusObject, foundDeviceIdx, replaceStr,
                record.extraInterfaces.at(key), _em.systemConfiguration);
        }

        // If we end up here and the path is empty, we have Probe: "True"
        // and we dont want that to show up in the associations.
        if (!path.empty())
        {
            auto boardName = record.name;
            std::string boardInventoryPath =
                em_utils::buildInventorySystemPath(boardName, record.type);
            _em.topology.addProbePath(boardInventoryPath, path);
        }

        // overwrite ourselves with cleaned up version
        _em.systemConfiguration.insert_or_assign(recordName, record);
        _missingConfigurations.erase(recordName);
    }
}

void scan::PerformScan::run()
{
    std::flat_set<std::string, std::less<>> dbusProbeInterfaces;
    std::vector<std::shared_ptr<probe::PerformProbe>> dbusProbePointers;

    for (auto it = _configurations.begin(); it != _configurations.end();)
    {
        if (std::find(passedProbes.begin(), passedProbes.end(), it->name) !=
            passedProbes.end())
        {
            it = _configurations.erase(it);
            continue;
        }

        EMConfig& recordRef = *it;
        std::vector<std::string> probeCommand = it->probeStmt;

        // store reference to this to children to makes sure we don't get
        // destroyed too early
        auto thisRef = shared_from_this();
        auto probePointer = std::make_shared<probe::PerformProbe>(
            recordRef, probeCommand, it->name, thisRef);

        // parse out dbus probes by discarding other probe types, store in a
        // map
        for (const std::string& probe : probeCommand)
        {
            if (probe::findProbeType(probe))
            {
                continue;
            }
            // syntax requires probe before first open brace
            auto findStart = probe.find('(');
            std::string interface = probe.substr(0, findStart);
            dbusProbeInterfaces.emplace(interface);
            dbusProbePointers.emplace_back(probePointer);
        }
        it++;
    }

    // probe vector stores a shared_ptr to each PerformProbe that cares
    // about a dbus interface
    findDbusObjects(std::move(dbusProbePointers),
                    std::move(dbusProbeInterfaces), shared_from_this(), io);
}

scan::PerformScan::~PerformScan()
{
    if (_passed)
    {
        auto nextScan = std::make_shared<PerformScan>(
            _em, _missingConfigurations, _configurations, io,
            std::move(_callback));
        nextScan->passedProbes = std::move(passedProbes);
        nextScan->dbusProbeObjects = std::move(dbusProbeObjects);
        boost::asio::post(_em.io, [nextScan]() { nextScan->run(); });
    }
    else
    {
        _callback();
    }
}
