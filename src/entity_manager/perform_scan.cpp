// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "perform_scan.hpp"

#include "perform_probe.hpp"
#include "utils.hpp"

#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <phosphor-logging/lg2.hpp>

#include <charconv>
#include <iostream>

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
        std::cerr << "retries exhausted on " << instance.busName << " "
                  << instance.path << " " << instance.interface << "\n";
        return;
    }

    scan->_em.systemBus->async_method_call(
        [instance, scan, probeVector, retries,
         &io](boost::system::error_code& errc, const DBusInterface& resp) {
            if (errc)
            {
                std::cerr << "error calling getall on  " << instance.busName
                          << " " << instance.path << " "
                          << instance.interface << "\n";

                auto timer = std::make_shared<boost::asio::steady_timer>(io);
                timer->expires_after(std::chrono::seconds(2));

                timer->async_wait([timer, instance, scan, probeVector, retries,
                                   &io](const boost::system::error_code&) {
                    getInterfaces(instance, probeVector, scan, io, retries - 1);
                });
                return;
            }

            scan->dbusProbeObjects[instance.path][instance.interface] = resp;
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
    boost::container::flat_set<std::string>&& interfaces,
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
                std::cerr << "Error communicating to mapper.\n";

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
        std::cerr << "Last JSON Illegal\n";
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
                                           nlohmann::json::iterator& keyPair)
{
    if (keyPair.value().is_string())
    {
        matches.emplace_back(keyPair.value());
        return true;
    }

    if (keyPair.value().is_array())
    {
        for (const auto& value : keyPair.value())
        {
            if (!value.is_string())
            {
                std::cerr << "Value is invalid type " << value << "\n";
                break;
            }
            matches.emplace_back(value);
        }

        return true;
    }

    std::cerr << "Value is invalid type " << keyPair.key() << "\n";

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

static void applyBindExposeAction(nlohmann::json& exposedObject,
                                  nlohmann::json& expose,
                                  const std::string& propertyName)
{
    if (propertyName.starts_with("Bind"))
    {
        std::string bind = propertyName.substr(sizeof("Bind") - 1);
        exposedObject["Status"] = "okay";
        expose[bind] = exposedObject;
    }
}

static void applyDisableExposeAction(nlohmann::json& exposedObject,
                                     const std::string& propertyName)
{
    if (propertyName == "DisableNode")
    {
        exposedObject["Status"] = "disabled";
    }
}

static void applyConfigExposeActions(
    std::vector<std::string>& matches, nlohmann::json& expose,
    const std::string& propertyName, nlohmann::json& configExposes)
{
    for (auto& exposedObject : configExposes)
    {
        auto match = findExposeActionRecord(matches, exposedObject);
        if (match)
        {
            matches.erase(*match);
            applyBindExposeAction(exposedObject, expose, propertyName);
            applyDisableExposeAction(exposedObject, propertyName);
        }
    }
}

static void applyExposeActions(
    nlohmann::json& systemConfiguration, const std::string& recordName,
    nlohmann::json& expose, nlohmann::json::iterator& keyPair)
{
    bool isBind = keyPair.key().starts_with("Bind");
    bool isDisable = keyPair.key() == "DisableNode";
    bool isExposeAction = isBind || isDisable;

    if (!isExposeAction)
    {
        return;
    }

    std::vector<std::string> matches;

    if (!extractExposeActionRecordNames(matches, keyPair))
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

        if (!configListFind->is_array())
        {
            continue;
        }

        applyConfigExposeActions(matches, expose, keyPair.key(),
                                 *configListFind);
    }

    if (!matches.empty())
    {
        std::cerr << "configuration file dependency error, could not find "
                  << keyPair.key() << " " << keyPair.value() << "\n";
    }
}

static std::string generateDeviceName(
    const std::set<nlohmann::json>& usedNames, const DBusObject& dbusObject,
    size_t foundDeviceIdx, const std::string& nameTemplate,
    std::optional<std::string>& replaceStr)
{
    nlohmann::json copyForName = {{"Name", nameTemplate}};
    nlohmann::json::iterator copyIt = copyForName.begin();
    std::optional<std::string> replaceVal = em_utils::templateCharReplace(
        copyIt, dbusObject, foundDeviceIdx, replaceStr);

    if (!replaceStr && replaceVal)
    {
        if (usedNames.contains(copyIt.value()))
        {
            replaceStr = replaceVal;
            copyForName = {{"Name", nameTemplate}};
            copyIt = copyForName.begin();
            em_utils::templateCharReplace(copyIt, dbusObject, foundDeviceIdx,
                                          replaceStr);
        }
    }

    if (replaceStr)
    {
        std::cerr << "Duplicates found, replacing " << *replaceStr
                  << " with found device index.\n Consider "
                     "fixing template to not have duplicates\n";
    }

    return copyIt.value();
}

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

        nlohmann::json record = recordRef;
        std::string recordName = getRecordName(foundDevice, probeName);
        size_t foundDeviceIdx = indexes.front();
        indexes.pop_front();

        // check name first so we have no duplicate names
        auto getName = record.find("Name");
        if (getName == record.end())
        {
            std::cerr << "Record Missing Name! " << record.dump();
            continue; // this should be impossible at this level
        }

        std::string deviceName = generateDeviceName(
            usedNames, dbusObject, foundDeviceIdx, getName.value(), replaceStr);
        getName.value() = deviceName;
        usedNames.insert(deviceName);

        for (auto keyPair = record.begin(); keyPair != record.end(); keyPair++)
        {
            if (keyPair.key() != "Name")
            {
                bool handleLeftOver = true;
                if ((keyPair.key() == "Probe") || (keyPair.key() == "Exposes"))
                {
                    // "Probe" string does not contain template variables
                    // Handle left-over variables for "Exposes" later below
                    handleLeftOver = false;
                }
                em_utils::templateCharReplace(keyPair, dbusObject,
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

        for (auto& expose : *findExpose)
        {
            for (auto keyPair = expose.begin(); keyPair != expose.end();
                 keyPair++)
            {
                em_utils::templateCharReplace(keyPair, dbusObject,
                                              foundDeviceIdx, replaceStr);

                applyExposeActions(_em.systemConfiguration, recordName, expose,
                                   keyPair);
            }
        }

        // overwrite ourselves with cleaned up version
        _em.systemConfiguration[recordName] = record;
        _missingConfigurations.erase(recordName);
    }
}

void scan::PerformScan::run()
{
    boost::container::flat_set<std::string> dbusProbeInterfaces;
    std::vector<std::shared_ptr<probe::PerformProbe>> dbusProbePointers;

    for (auto it = _configurations.begin(); it != _configurations.end();)
    {
        // check for poorly formatted fields, probe must be an array
        auto findProbe = it->find("Probe");
        if (findProbe == it->end())
        {
            std::cerr << "configuration file missing probe:\n " << *it << "\n";
            it = _configurations.erase(it);
            continue;
        }

        auto findName = it->find("Name");
        if (findName == it->end())
        {
            std::cerr << "configuration file missing name:\n " << *it << "\n";
            it = _configurations.erase(it);
            continue;
        }
        std::string probeName = *findName;

        if (std::find(passedProbes.begin(), passedProbes.end(), probeName) !=
            passedProbes.end())
        {
            it = _configurations.erase(it);
            continue;
        }

        nlohmann::json& recordRef = *it;
        nlohmann::json probeCommand;
        if ((*findProbe).type() != nlohmann::json::value_t::array)
        {
            probeCommand = nlohmann::json::array();
            probeCommand.push_back(*findProbe);
        }
        else
        {
            probeCommand = *findProbe;
        }

        // store reference to this to children to makes sure we don't get
        // destroyed too early
        auto thisRef = shared_from_this();
        auto probePointer = std::make_shared<probe::PerformProbe>(
            recordRef, probeCommand, probeName, thisRef);

        // parse out dbus probes by discarding other probe types, store in a
        // map
        for (const nlohmann::json& probeJson : probeCommand)
        {
            const std::string* probe = probeJson.get_ptr<const std::string*>();
            if (probe == nullptr)
            {
                std::cerr << "Probe statement wasn't a string, can't parse";
                continue;
            }
            if (probe::findProbeType(*probe))
            {
                continue;
            }
            // syntax requires probe before first open brace
            auto findStart = probe->find('(');
            std::string interface = probe->substr(0, findStart);
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
        nextScan->run();
    }
    else
    {
        _callback();
    }
}
