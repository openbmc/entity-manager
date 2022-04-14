/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
/// \file PerformScan.cpp
#include "EntityManager.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>

#include <charconv>

/* Hacks from splitting EntityManager.cpp */
extern std::shared_ptr<sdbusplus::asio::connection> systemBus;
extern nlohmann::json lastJson;
extern void
    propertiesChangedCallback(nlohmann::json& systemConfiguration,
                              sdbusplus::asio::object_server& objServer);

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

constexpr const int32_t maxMapperDepth = 0;

constexpr const bool debug = false;

struct DBusInterfaceInstance
{
    std::string busName;
    std::string path;
    std::string interface;
};

void getInterfaces(
    const DBusInterfaceInstance& instance,
    const std::vector<std::shared_ptr<PerformProbe>>& probeVector,
    const std::shared_ptr<PerformScan>& scan, size_t retries = 5)
{
    if (!retries)
    {
        std::cerr << "retries exhausted on " << instance.busName << " "
                  << instance.path << " " << instance.interface << "\n";
        return;
    }

    systemBus->async_method_call(
        [instance, scan, probeVector, retries](boost::system::error_code& errc,
                                               const DBusInterface& resp) {
            if (errc)
            {
                std::cerr << "error calling getall on  " << instance.busName
                          << " " << instance.path << " "
                          << instance.interface << "\n";

                auto timer = std::make_shared<boost::asio::steady_timer>(io);
                timer->expires_after(std::chrono::seconds(2));

                timer->async_wait([timer, instance, scan, probeVector,
                                   retries](const boost::system::error_code&) {
                    getInterfaces(instance, probeVector, scan, retries - 1);
                });
                return;
            }

            scan->dbusProbeObjects[instance.path][instance.interface] = resp;
        },
        instance.busName, instance.path, "org.freedesktop.DBus.Properties",
        "GetAll", instance.interface);

    if constexpr (debug)
    {
        std::cerr << __func__ << " " << __LINE__ << "\n";
    }
}

static void registerCallback(nlohmann::json& systemConfiguration,
                             sdbusplus::asio::object_server& objServer,
                             const std::string& path)
{
    static boost::container::flat_map<std::string, sdbusplus::bus::match::match>
        dbusMatches;

    auto find = dbusMatches.find(path);
    if (find != dbusMatches.end())
    {
        return;
    }

    std::function<void(sdbusplus::message::message & message)> eventHandler =
        [&](sdbusplus::message::message&) {
            propertiesChangedCallback(systemConfiguration, objServer);
        };

    sdbusplus::bus::match::match match(
        static_cast<sdbusplus::bus::bus&>(*systemBus),
        "type='signal',member='PropertiesChanged',path='" + path + "'",
        eventHandler);
    dbusMatches.emplace(path, std::move(match));
}

static void
    processDbusObjects(std::vector<std::shared_ptr<PerformProbe>>& probeVector,
                       const std::shared_ptr<PerformScan>& scan,
                       const GetSubTreeType& interfaceSubtree)
{
    for (const auto& [path, object] : interfaceSubtree)
    {
        // Get a PropertiesChanged callback for all interfaces on this path.
        registerCallback(scan->_systemConfiguration, scan->objServer, path);

        for (const auto& [busname, ifaces] : object)
        {
            for (const std::string& iface : ifaces)
            {
                // The 3 default org.freedeskstop interfaces (Peer,
                // Introspectable, and Properties) are returned by
                // the mapper but don't have properties, so don't bother
                // with the GetAll call to save some cycles.
                if (!boost::algorithm::starts_with(iface, "org.freedesktop"))
                {
                    getInterfaces({busname, path, iface}, probeVector, scan);
                }
            }
        }
    }
}

// Populates scan->dbusProbeObjects with all interfaces and properties
// for the paths that own the interfaces passed in.
void findDbusObjects(std::vector<std::shared_ptr<PerformProbe>>&& probeVector,
                     boost::container::flat_set<std::string>&& interfaces,
                     const std::shared_ptr<PerformScan>& scan,
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
    systemBus->async_method_call(
        [interfaces, probeVector{std::move(probeVector)}, scan,
         retries](boost::system::error_code& ec,
                  const GetSubTreeType& interfaceSubtree) mutable {
            if (ec)
            {
                if (ec.value() == ENOENT)
                {
                    return; // wasn't found by mapper
                }
                std::cerr << "Error communicating to mapper.\n";

                if (!retries)
                {
                    // if we can't communicate to the mapper something is very
                    // wrong
                    std::exit(EXIT_FAILURE);
                }

                auto timer = std::make_shared<boost::asio::steady_timer>(io);
                timer->expires_after(std::chrono::seconds(10));

                timer->async_wait(
                    [timer, interfaces{std::move(interfaces)}, scan,
                     probeVector{std::move(probeVector)},
                     retries](const boost::system::error_code&) mutable {
                        findDbusObjects(std::move(probeVector),
                                        std::move(interfaces), scan,
                                        retries - 1);
                    });
                return;
            }

            processDbusObjects(probeVector, scan, interfaceSubtree);
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree", "/", maxMapperDepth,
        interfaces);

    if constexpr (debug)
    {
        std::cerr << __func__ << " " << __LINE__ << "\n";
    }
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
    for (auto& devPair : probe)
    {
        device.push_back(devPair.first);
        std::visit([&device](auto&& v) { device.push_back(v); },
                   devPair.second);
    }

    // hashes are hard to distinguish, use the non-hashed version if we want
    // debug
    if constexpr (debug)
    {
        return probeName + device.dump();
    }

    return std::to_string(std::hash<std::string>{}(probeName + device.dump()));
}

PerformScan::PerformScan(nlohmann::json& systemConfiguration,
                         nlohmann::json& missingConfigurations,
                         std::list<nlohmann::json>& configurations,
                         sdbusplus::asio::object_server& objServerIn,
                         std::function<void()>&& callback) :
    _systemConfiguration(systemConfiguration),
    _missingConfigurations(missingConfigurations),
    _configurations(configurations), objServer(objServerIn),
    _callback(std::move(callback))
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

static void recordDiscoveredIdentifiers(std::set<nlohmann::json>& usedNames,
                                        std::list<size_t>& indexes,
                                        const std::string& probeName,
                                        const nlohmann::json& record)
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
    auto [p, ec] = std::from_chars(str.data(), str.data() + str.size(), index);
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

static std::optional<std::vector<std::string>::iterator>
    findExposeActionRecord(std::vector<std::string>& matches,
                           const nlohmann::json& record)
{
    const auto& name = (record)["Name"].get<std::string>();
    auto compare = [name](const std::string& s) { return s == name; };
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
    if (boost::starts_with(propertyName, "Bind"))
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

static void applyConfigExposeActions(std::vector<std::string>& matches,
                                     nlohmann::json& expose,
                                     const std::string& propertyName,
                                     nlohmann::json& configExposes)
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

static void applyExposeActions(nlohmann::json& systemConfiguration,
                               const std::string& recordName,
                               nlohmann::json& expose,
                               nlohmann::json::iterator& keyPair)
{
    bool isBind = boost::starts_with(keyPair.key(), "Bind");
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

    for (auto& [configId, config] : systemConfiguration.items())
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

void PerformScan::run()
{
    boost::container::flat_set<std::string> dbusProbeInterfaces;
    std::vector<std::shared_ptr<PerformProbe>> dbusProbePointers;

    for (auto it = _configurations.begin(); it != _configurations.end();)
    {
        auto findProbe = it->find("Probe");
        auto findName = it->find("Name");

        nlohmann::json probeCommand;
        // check for poorly formatted fields, probe must be an array
        if (findProbe == it->end())
        {
            std::cerr << "configuration file missing probe:\n " << *it << "\n";
            it = _configurations.erase(it);
            continue;
        }
        if ((*findProbe).type() != nlohmann::json::value_t::array)
        {
            probeCommand = nlohmann::json::array();
            probeCommand.push_back(*findProbe);
        }
        else
        {
            probeCommand = *findProbe;
        }

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
        nlohmann::json* recordPtr = &(*it);

        // store reference to this to children to makes sure we don't get
        // destroyed too early
        auto thisRef = shared_from_this();
        auto probePointer = std::make_shared<PerformProbe>(
            probeCommand, thisRef,
            [&, recordPtr,
             probeName](FoundDevices& foundDevices,
                        const MapperGetSubTreeResponse& dbusSubtree) {
                _passed = true;
                std::set<nlohmann::json> usedNames;
                passedProbes.push_back(probeName);
                std::list<size_t> indexes(foundDevices.size());
                std::iota(indexes.begin(), indexes.end(), 1);

                // copy over persisted configurations and make sure we remove
                // indexes that are already used
                for (auto itr = foundDevices.begin();
                     itr != foundDevices.end();)
                {
                    std::string recordName =
                        getRecordName(itr->interface, probeName);

                    auto record = lastJson.find(recordName);
                    if (record == lastJson.end())
                    {
                        itr++;
                        continue;
                    }

                    pruneRecordExposes(*record);

                    recordDiscoveredIdentifiers(usedNames, indexes, probeName,
                                                *record);

                    // keep user changes
                    _systemConfiguration[recordName] = *record;
                    _missingConfigurations.erase(recordName);

                    // We've processed the device, remove it and advance the
                    // iterator
                    itr = foundDevices.erase(itr);
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
                    auto objectIt = dbusSubtree.find(path);
                    const DBusObject& dbusObject =
                        (objectIt == dbusSubtree.end()) ? emptyObject
                                                        : objectIt->second;

                    nlohmann::json record = *recordPtr;
                    std::string recordName =
                        getRecordName(foundDevice, probeName);
                    size_t foundDeviceIdx = indexes.front();
                    indexes.pop_front();

                    // check name first so we have no duplicate names
                    auto getName = record.find("Name");
                    if (getName == record.end())
                    {
                        std::cerr << "Record Missing Name! " << record.dump();
                        continue; // this should be impossible at this level
                    }

                    nlohmann::json copyForName = {{"Name", getName.value()}};
                    nlohmann::json::iterator copyIt = copyForName.begin();
                    std::optional<std::string> replaceVal = templateCharReplace(
                        copyIt, dbusObject, foundDeviceIdx, replaceStr);

                    if (!replaceStr && replaceVal)
                    {
                        if (usedNames.find(copyIt.value()) != usedNames.end())
                        {
                            replaceStr = replaceVal;
                            copyForName = {{"Name", getName.value()}};
                            copyIt = copyForName.begin();
                            templateCharReplace(copyIt, dbusObject,
                                                foundDeviceIdx, replaceStr);
                        }
                    }

                    if (replaceStr)
                    {
                        std::cerr << "Duplicates found, replacing "
                                  << *replaceStr
                                  << " with found device index.\n Consider "
                                     "fixing template to not have duplicates\n";
                    }

                    getName.value() = copyIt.value();
                    usedNames.insert(copyIt.value());

                    for (auto keyPair = record.begin(); keyPair != record.end();
                         keyPair++)
                    {
                        if (keyPair.key() != "Name")
                        {
                            templateCharReplace(keyPair, dbusObject,
                                                foundDeviceIdx, replaceStr);
                        }
                    }

                    // insert into configuration temporarily to be able to
                    // reference ourselves

                    _systemConfiguration[recordName] = record;

                    auto findExpose = record.find("Exposes");
                    if (findExpose == record.end())
                    {
                        continue;
                    }

                    for (auto& expose : *findExpose)
                    {
                        for (auto keyPair = expose.begin();
                             keyPair != expose.end(); keyPair++)
                        {

                            templateCharReplace(keyPair, dbusObject,
                                                foundDeviceIdx, replaceStr);

                            applyExposeActions(_systemConfiguration, recordName,
                                               expose, keyPair);
                        }
                    }
                    // overwrite ourselves with cleaned up version
                    _systemConfiguration[recordName] = record;
                    _missingConfigurations.erase(recordName);
                }
            });

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
            if (findProbeType(probe->c_str()))
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
                    std::move(dbusProbeInterfaces), shared_from_this());
    if constexpr (debug)
    {
        std::cerr << __func__ << " " << __LINE__ << "\n";
    }
}

PerformScan::~PerformScan()
{
    if (_passed)
    {
        auto nextScan = std::make_shared<PerformScan>(
            _systemConfiguration, _missingConfigurations, _configurations,
            objServer, std::move(_callback));
        nextScan->passedProbes = std::move(passedProbes);
        nextScan->dbusProbeObjects = std::move(dbusProbeObjects);
        nextScan->run();

        if constexpr (debug)
        {
            std::cerr << __func__ << " " << __LINE__ << "\n";
        }
    }
    else
    {
        _callback();

        if constexpr (debug)
        {
            std::cerr << __func__ << " " << __LINE__ << "\n";
        }
    }
}
