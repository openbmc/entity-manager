// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "perform_probe.hpp"

#include "perform_scan.hpp"
#include "probe_type.hpp"

#include <phosphor-logging/lg2.hpp>

#include <regex>
#include <utility>

// probes dbus interface dictionary for a key with a value that matches a regex
// When an interface passes a probe, also save its D-Bus path with it.
bool probeDbus(const std::string& interfaceName,
               const std::map<std::string, nlohmann::json>& matches,
               scan::FoundDevices& devices,
               const std::shared_ptr<scan::PerformScan>& scan, bool& foundProbe)
{
    bool foundMatch = false;
    foundProbe = false;

    for (const auto& [path, interfaces] : scan->dbusProbeObjects)
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

// default probe entry point, iterates a list looking for specific types to
// call specific probe functions
bool doProbe(const std::vector<std::string>& probeCommand,
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
                        lg2::error("found probe syntax error {JSON}", "JSON",
                                   probe);
                        return false;
                    }
                    std::string commandStr = *(match.begin() + 1);
                    replaceAll(commandStr, "'", "");

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
                lg2::error("dbus probe syntax error {JSON}", "JSON", probe);
                return false;
            }
            std::string commandStr = *(match.begin() + 1);
            // convert single ticks and single slashes into legal json
            std::ranges::replace(commandStr, '\'', '"');

            replaceAll(commandStr, R"(\)", R"(\\)");
            auto json = nlohmann::json::parse(commandStr, nullptr, false, true);
            if (json.is_discarded())
            {
                lg2::error("dbus command syntax error {STR}", "STR",
                           commandStr);
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
            cur = probeDbus(probeInterface, dbusProbeMap, foundDevs, scan,
                            foundProbe);
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
            std::flat_map<std::string, DBusValueVariant, std::less<>>{},
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

namespace probe
{

PerformProbe::PerformProbe(nlohmann::json& recordRef,
                           const std::vector<std::string>& probeCommand,
                           std::string probeName,
                           std::shared_ptr<scan::PerformScan>& scanPtr) :
    recordRef(recordRef), _probeCommand(probeCommand),
    probeName(std::move(probeName)), scan(scanPtr)
{}

PerformProbe::~PerformProbe()
{
    scan::FoundDevices foundDevs;
    if (doProbe(_probeCommand, scan, foundDevs))
    {
        scan->updateSystemConfiguration(recordRef, probeName, foundDevs);
    }
}

} // namespace probe
