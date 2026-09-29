// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "perform_probe.hpp"

#include "perform_scan.hpp"
#include "probe_lexer.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

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
bool doProbe(const std::vector<probe::Token>& probeCommand,
             const std::shared_ptr<scan::PerformScan>& scan,
             scan::FoundDevices& foundDevs)
{
    bool ret = false;
    bool matchOne = false;
    bool cur = true;
    probe::TokenType lastCommand = probe::TokenType::boolFalse;
    bool first = true;

    for (const probe::Token& token : probeCommand)
    {
        switch (token.type)
        {
            case probe::TokenType::boolFalse:
            {
                cur = false;
                break;
            }
            case probe::TokenType::boolTrue:
            {
                cur = true;
                break;
            }
            case probe::TokenType::matchOne:
            {
                // does not affect the outcome; carry the running result
                cur = ret;
                matchOne = true;
                break;
            }
            case probe::TokenType::opAnd:
            case probe::TokenType::opOr:
            {
                // no-ops here; applied via lastCommand on the next operand
                break;
            }
            case probe::TokenType::found:
            {
                cur = std::ranges::contains(scan->passedProbes, token.value);
                break;
            }
            case probe::TokenType::dbusProbe:
            {
                if (token.dbus)
                {
                    // we can match any (string, variant) property. (string,
                    // string) does a regex
                    bool foundProbe = false;
                    cur = probeDbus(token.dbus->interface,
                                    token.dbus->properties, foundDevs, scan,
                                    foundProbe);
                    break;
                }
            }
        }

        // AND and OR only take effect on the operand that follows them
        if (lastCommand == probe::TokenType::opAnd)
        {
            ret = cur && ret;
        }
        else if (lastCommand == probe::TokenType::opOr)
        {
            ret = cur || ret;
        }

        if (first)
        {
            ret = cur;
            first = false;
        }
        lastCommand = token.type;
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

PerformProbe::PerformProbe(EMConfig& recordRef,
                           const std::vector<Token>& probeCommand,
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
