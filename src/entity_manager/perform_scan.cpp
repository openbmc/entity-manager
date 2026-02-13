// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "perform_scan.hpp"

#include "perform_probe.hpp"
#include "utils.hpp"

#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>

#include <algorithm>
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
    EntityManager& em, nlohmann::json& missingConfigurations,
    std::vector<nlohmann::json>& configurations, boost::asio::io_context& io,
    std::function<void()>&& callback) :
    _em(em), _missingConfigurations(missingConfigurations),
    _configurations(configurations), _callback(std::move(callback)), io(io)
{}

static void pruneRecordExposes(nlohmann::json& record)
{
    auto findExposes = record.find("Exposes");
    if (findExposes == record.end())
    {
        return;
    }

    auto copy = nlohmann::json::array();
    for (auto& expose : *findExposes)
    {
        if (!expose.is_null())
        {
            copy.emplace_back(expose);
        }
    }
    *findExposes = copy;
}

static void recordDiscoveredIdentifiers(
    std::set<nlohmann::json>& usedNames, std::list<size_t>& indexes,
    const std::string& probeName, const nlohmann::json& record)
{
    size_t indexIdx = probeName.find('$');
    if (indexIdx == std::string::npos)
    {
        return;
    }

    auto nameIt = record.find("Name");
    if (nameIt == record.end())
    {
        lg2::error("Last JSON Illegal");
        return;
    }

    int index = 0;
    auto str = nameIt->get<std::string>().substr(indexIdx);
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const char* endPtr = str.data() + str.size();
    auto [p, ec] = std::from_chars(str.data(), endPtr, index);
    if (ec != std::errc())
    {
        return; // non-numeric replacement
    }

    usedNames.insert(nameIt.value());

    auto usedIt = std::find(indexes.begin(), indexes.end(), index);
    if (usedIt != indexes.end())
    {
        indexes.erase(usedIt);
    }
}

static bool extractExposeActionRecordNames(std::vector<std::string>& matches,
                                           const std::string& exposeKey,
                                           nlohmann::json& exposeValue)
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
                lg2::error("Value is invalid type {VALUE}", "VALUE", value);
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
    const std::string& propertyName, nlohmann::json::array_t& configExposes)
{
    for (auto& exposedObject : configExposes)
    {
        auto match = findExposeActionRecord(matches, exposedObject);
        if (match)
        {
            matches.erase(*match);
            nlohmann::json::object_t* exposedObjectObj =
                exposedObject.get_ptr<nlohmann::json::object_t*>();
            if (exposedObjectObj == nullptr)
            {
                lg2::error("Exposed object wasn't a object: {JSON}", "JSON",
                           exposedObject.dump());
                continue;
            }

            applyBindExposeAction(*exposedObjectObj, expose, propertyName);
            applyDisableExposeAction(*exposedObjectObj, propertyName);
        }
    }
}

static void applyExposeActions(
    nlohmann::json& systemConfiguration, const std::string& recordName,
    nlohmann::json::object_t& expose, const std::string& exposeKey,
    nlohmann::json& exposeValue)
{
    bool isBind = exposeKey.starts_with("Bind");
    bool isDisable = exposeKey == "DisableNode";
    bool isExposeAction = isBind || isDisable;

    if (!isExposeAction)
    {
        return;
    }

    std::vector<std::string> matches;

    if (!extractExposeActionRecordNames(matches, exposeKey, exposeValue))
    {
        return;
    }

    for (const auto& [configId, config] : systemConfiguration.items())
    {
        // don't disable ourselves
        if (isDisable && configId == recordName)
        {
            continue;
        }

        auto configListFind = config.find("Exposes");
        if (configListFind == config.end())
        {
            continue;
        }

        nlohmann::json::array_t* configList =
            configListFind->get_ptr<nlohmann::json::array_t*>();
        if (configList == nullptr)
        {
            continue;
        }
        applyConfigExposeActions(matches, expose, exposeKey, *configList);
    }

    if (!matches.empty())
    {
        lg2::error(
            "configuration file dependency error, could not find {KEY} {VALUE}",
            "KEY", exposeKey, "VALUE", exposeValue);
    }
}

static std::string generateDeviceName(
    const std::set<nlohmann::json>& usedNames, const DBusObject& dbusObject,
    size_t foundDeviceIdx, const std::string& nameTemplate,
    std::optional<std::string>& replaceStr)
{
    nlohmann::json copyForName = nameTemplate;
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
    const std::string* ret = copyForName.get_ptr<const std::string*>();
    if (ret == nullptr)
    {
        lg2::error("Device name wasn't a string: ${JSON}", "JSON",
                   copyForName.dump());
        return "";
    }
    return *ret;
}
static void applyTemplateAndExposeActions(
    const std::string& recordName, const DBusObject& dbusObject,
    size_t foundDeviceIdx, const std::optional<std::string>& replaceStr,
    nlohmann::json& value, nlohmann::json& systemConfiguration)
{
    nlohmann::json::object_t* exposeObj =
        value.get_ptr<nlohmann::json::object_t*>();
    if (exposeObj == nullptr)
    {
        return;
    }
    for (auto& [key, value] : *exposeObj)
    {
        em_utils::templateCharReplace(value, dbusObject, foundDeviceIdx,
                                      replaceStr);

        applyExposeActions(systemConfiguration, recordName, *exposeObj, key,
                           value);
    }
};

void scan::PerformScan::updateSystemConfiguration(
    const nlohmann::json& recordRef, const std::string& probeName,
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

        auto record = _em.systemConfiguration.find(recordName);
        if (record == _em.systemConfiguration.end())
        {
            record = _em.lastJson.find(recordName);
            if (record == _em.lastJson.end())
            {
                itr++;
                continue;
            }

            pruneRecordExposes(*record);

            _em.systemConfiguration[recordName] = *record;
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

        const nlohmann::json::object_t* recordPtr =
            recordRef.get_ptr<const nlohmann::json::object_t*>();
        if (recordPtr == nullptr)
        {
            lg2::error("Failed to parse record {JSON}", "JSON",
                       recordRef.dump());
            continue;
        }
        nlohmann::json::object_t record = *recordPtr;
        std::string recordName = getRecordName(foundDevice, probeName);
        size_t foundDeviceIdx = indexes.front();
        indexes.pop_front();

        // check name first so we have no duplicate names
        auto getName = record.find("Name");
        if (getName == record.end())
        {
            lg2::error("Record Missing Name! {JSON}", "JSON", recordRef.dump());
            continue; // this should be impossible at this level
        }

        const std::string* name = getName->second.get_ptr<const std::string*>();
        if (name == nullptr)
        {
            lg2::error("Name wasn't a string: {JSON}", "JSON",
                       recordRef.dump());
            continue;
        }

        std::string deviceName = generateDeviceName(
            usedNames, dbusObject, foundDeviceIdx, *name, replaceStr);

        record["Name"] = deviceName;

        usedNames.insert(deviceName);

        // So FOUND('resolved name') can match; we already added probeName
        // (template) at the start
        passedProbes.push_back(deviceName);

        for (auto& keyPair : record)
        {
            if (keyPair.first != "Name")
            {
                // "Probe" string does not contain template variables
                // Handle left-over variables for "Exposes" later below
                const bool handleLeftOver =
                    (keyPair.first != "Probe") && (keyPair.first != "Exposes");
                em_utils::templateCharReplace(keyPair.second, dbusObject,
                                              foundDeviceIdx, replaceStr,
                                              handleLeftOver);
            }
        }

        // insert into configuration temporarily to be able to
        // reference ourselves

        _em.systemConfiguration[recordName] = record;

        auto findExpose = record.find("Exposes");
        if (findExpose == record.end())
        {
            continue;
        }

        nlohmann::json::array_t* exposeArr =
            findExpose->second.get_ptr<nlohmann::json::array_t*>();
        if (exposeArr != nullptr)
        {
            for (auto& value : *exposeArr)
            {
                applyTemplateAndExposeActions(recordName, dbusObject,
                                              foundDeviceIdx, replaceStr, value,
                                              _em.systemConfiguration);
            }
        }
        else
        {
            applyTemplateAndExposeActions(
                recordName, dbusObject, foundDeviceIdx, replaceStr,
                findExpose->second, _em.systemConfiguration);
        }

        // If we end up here and the path is empty, we have Probe: "True"
        // and we dont want that to show up in the associations.
        if (!path.empty())
        {
            auto boardType = record.find("Type")->second.get<std::string>();
            auto boardName = record.find("Name")->second.get<std::string>();
            std::string boardInventoryPath =
                em_utils::buildInventorySystemPath(boardName, boardType);
            _em.topology.addProbePath(boardInventoryPath, path);
        }

        // overwrite ourselves with cleaned up version
        _em.systemConfiguration[recordName] = record;
        _missingConfigurations.erase(recordName);
    }
}

void scan::PerformScan::run()
{
    // Seed from systemConfiguration so we don't prune configs that are
    // already applied.
    for (const auto& [_, config] : _em.systemConfiguration.items())
    {
        auto nameIt = config.find("Name");
        if (nameIt != config.end() && nameIt->is_string())
        {
            passedProbes.push_back(nameIt->get<std::string>());
        }
    }

    // Remove from missingConfigurations any config whose Name is in
    // passedProbes (e.g. from previous scan or just seeded above), so the
    // callback won't prune them.
    for (const std::string& name : passedProbes)
    {
        for (auto mit = _missingConfigurations.begin();
             mit != _missingConfigurations.end();)
        {
            const auto& dev = mit.value();
            auto nameIt = dev.find("Name");
            if (nameIt != dev.end() && nameIt->is_string() &&
                nameIt->get<std::string>() == name)
            {
                mit = _missingConfigurations.erase(mit);
            }
            else
            {
                ++mit;
            }
        }
    }

    std::flat_set<std::string, std::less<>> dbusProbeInterfaces;
    std::vector<std::shared_ptr<probe::PerformProbe>> dbusProbePointers;

    for (auto it = _configurations.begin(); it != _configurations.end();)
    {
        // check for poorly formatted fields, probe must be an array
        auto findProbe = it->find("Probe");
        if (findProbe == it->end())
        {
            lg2::error("configuration file missing probe:\n {JSON}", "JSON",
                       *it);
            it = _configurations.erase(it);
            continue;
        }

        auto findName = it->find("Name");
        if (findName == it->end())
        {
            lg2::error("configuration file missing name:\n {JSON}", "JSON",
                       *it);
            it = _configurations.erase(it);
            continue;
        }

        const std::string* probeName = findName->get_ptr<const std::string*>();
        if (probeName == nullptr)
        {
            lg2::error("Name wasn't a string? {JSON}", "JSON", *it);
            it = _configurations.erase(it);
            continue;
        }

        if (std::find(passedProbes.begin(), passedProbes.end(), *probeName) !=
            passedProbes.end())
        {
            it = _configurations.erase(it);
            continue;
        }

        nlohmann::json& recordRef = *it;
        std::vector<std::string> probeCommand;
        nlohmann::json::array_t* probeCommandArrayPtr =
            findProbe->get_ptr<nlohmann::json::array_t*>();
        if (probeCommandArrayPtr != nullptr)
        {
            for (const auto& probe : *probeCommandArrayPtr)
            {
                const std::string* probeStr =
                    probe.get_ptr<const std::string*>();
                if (probeStr == nullptr)
                {
                    lg2::error("Probe statement wasn't a string, can't parse");
                    return;
                }
                probeCommand.push_back(*probeStr);
            }
        }
        else
        {
            const std::string* probeStr =
                findProbe->get_ptr<const std::string*>();
            if (probeStr == nullptr)
            {
                lg2::error("Probe statement wasn't a string, can't parse");
                return;
            }
            probeCommand.push_back(*probeStr);
        }

        // store reference to this to children to makes sure we don't get
        // destroyed too early
        auto thisRef = shared_from_this();
        auto probePointer = std::make_shared<probe::PerformProbe>(
            recordRef, probeCommand, *probeName, thisRef);

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
