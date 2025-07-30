#include "system_mapper.hpp"

#include "entity_manager.hpp"
#include "inventory_manager.hpp"
#include "object_mapper.hpp"

#include <boost/algorithm/string/replace.hpp>
#include <phosphor-logging/lg2.hpp>

#include <iostream>
#include <regex>

PHOSPHOR_LOG2_USING;

SystemMapper::SystemMapper(
    EntityManager& em, boost::asio::io_context& io,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    std::shared_ptr<sdbusplus::asio::object_server>& objServer) :
    inventoryManager(io, systemBus, objServer, *this),
    lastJson(nlohmann::json::object()),
    systemConfiguration(nlohmann::json::object()),
    objectMapper(io, systemBus, *this), entityManager(em)
{}

void SystemMapper::processDbusObjects(
    std::vector<std::shared_ptr<probe::PerformProbe>>& probeVector,
    const GetSubTreeType& interfaceSubtree)
{
    for (const auto& [path, object] : interfaceSubtree)
    {
        // Get a PropertiesChanged callback for all interfaces on this path.
        entityManager.registerCallback(path);

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
                    objectMapper.getInterfaceProperties(
                        {busname, path, iface}, probeVector, dbusProbeObjects);
                }
            }
        }
    }
}

// default probe entry point, iterates a list looking for specific types to
// call specific probe functions
bool SystemMapper::doProbe(const std::vector<std::string>& probeCommand,
                           const std::shared_ptr<scan::PerformScan>& scan,
                           scan::FoundDevices& foundDevs)
{
    const static std::regex command(R"(\((.*)\))");
    std::smatch match;
    bool ret = false;
    bool matchOne = false;
    bool cur = true;
    probe::probe_type_codes lastCommand = probe::probe_type_codes::FALSE_T;
    bool first = true;

    for (const auto& probe : probeCommand)
    {
        probe::FoundProbeTypeT probeType = probe::findProbeType(probe);
        if (probeType)
        {
            switch (*probeType)
            {
                case probe::probe_type_codes::FALSE_T:
                {
                    cur = false;
                    break;
                }
                case probe::probe_type_codes::TRUE_T:
                {
                    cur = true;
                    break;
                }
                case probe::probe_type_codes::MATCH_ONE:
                {
                    // set current value to last, this probe type shouldn't
                    // affect the outcome
                    cur = ret;
                    matchOne = true;
                    break;
                }
                /*case probe::probe_type_codes::AND:
                  break;
                case probe::probe_type_codes::OR:
                  break;
                  // these are no-ops until the last command switch
                  */
                case probe::probe_type_codes::FOUND:
                {
                    if (!std::regex_search(probe, match, command))
                    {
                        std::cerr
                            << "found probe syntax error " << probe << "\n";
                        return false;
                    }
                    std::string commandStr = *(match.begin() + 1);
                    boost::replace_all(commandStr, "'", "");
                    cur = (std::find(scan->passedProbes.begin(),
                                     scan->passedProbes.end(), commandStr) !=
                           scan->passedProbes.end());
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
        // look on dbus for object
        else
        {
            if (!std::regex_search(probe, match, command))
            {
                std::cerr << "dbus probe syntax error " << probe << "\n";
                return false;
            }
            std::string commandStr = *(match.begin() + 1);
            // convert single ticks and single slashes into legal json
            boost::replace_all(commandStr, "'", "\"");
            boost::replace_all(commandStr, R"(\)", R"(\\)");
            auto json = nlohmann::json::parse(commandStr, nullptr, false, true);
            if (json.is_discarded())
            {
                std::cerr << "dbus command syntax error " << commandStr << "\n";
                return false;
            }
            // we can match any (string, variant) property. (string, string)
            // does a regex
            std::map<std::string, nlohmann::json> dbusProbeMap =
                json.get<std::map<std::string, nlohmann::json>>();
            auto findStart = probe.find('(');
            if (findStart == std::string::npos)
            {
                return false;
            }
            bool foundProbe = !!probeType;
            std::string probeInterface = probe.substr(0, findStart);
            cur =
                probeDbus(probeInterface, dbusProbeMap, foundDevs, foundProbe);
        }

        // some functions like AND and OR only take affect after the
        // fact
        if (lastCommand == probe::probe_type_codes::AND)
        {
            ret = cur && ret;
        }
        else if (lastCommand == probe::probe_type_codes::OR)
        {
            ret = cur || ret;
        }

        if (first)
        {
            ret = cur;
            first = false;
        }
        lastCommand = probeType.value_or(probe::probe_type_codes::FALSE_T);
    }

    // probe passed, but empty device
    if (ret && foundDevs.empty())
    {
        foundDevs.emplace_back(
            boost::container::flat_map<std::string, DBusValueVariant>{},
            std::string{});
    }
    if (matchOne && ret)
    {
        // match the last one
        auto last = foundDevs.back();
        foundDevs.clear();

        foundDevs.emplace_back(std::move(last));
    }
    return ret;
}

// probes dbus interface dictionary for a key with a value that matches a regex
// When an interface passes a probe, also save its D-Bus path with it.
bool SystemMapper::probeDbus(
    const std::string& interfaceName,
    const std::map<std::string, nlohmann::json>& matches,
    scan::FoundDevices& devices, bool& foundProbe)
{
    bool foundMatch = false;
    foundProbe = false;

    for (const auto& [path, interfaces] : dbusProbeObjects)
    {
        auto it = interfaces.find(interfaceName);
        if (it == interfaces.end())
        {
            continue;
        }

        foundProbe = true;

        bool deviceMatches = true;
        const DBusInterface& interface = it->second;

        for (const auto& [matchProp, matchJSON] : matches)
        {
            auto deviceValue = interface.find(matchProp);
            if (deviceValue != interface.end())
            {
                deviceMatches = deviceMatches &&
                                matchProbe(matchJSON, deviceValue->second);
            }
            else
            {
                // Move on to the next DBus path
                deviceMatches = false;
                break;
            }
        }
        if (deviceMatches)
        {
            lg2::debug("Found probe match on {PATH} {IFACE}", "PATH", path,
                       "IFACE", interfaceName);
            devices.emplace_back(interface, path);
            foundMatch = true;
        }
    }
    return foundMatch;
}
