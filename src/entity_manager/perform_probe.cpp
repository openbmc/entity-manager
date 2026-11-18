// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "perform_probe.hpp"

#include "perform_scan.hpp"
#include "probe_lexer.hpp"
#include "probe_parser.hpp"

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
               const MapperGetSubTreeResponse& dbusProbeObjects,
               bool& foundProbe)
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

// Evaluates one node of the tree. AND and OR evaluate both children, the left
// one first, so a D-Bus probe collects its devices even when the result is
// already known.
static bool evaluateNode(const probe::Node& node,
                         const MapperGetSubTreeResponse& dbusProbeObjects,
                         const std::vector<std::string>& passedProbes,
                         scan::FoundDevices& foundDevs)
{
    switch (node.token.type)
    {
        case probe::TokenType::boolFalse:
        {
            return false;
        }
        case probe::TokenType::boolTrue:
        {
            return true;
        }
        case probe::TokenType::found:
        {
            return std::ranges::contains(passedProbes, node.token.value);
        }
        case probe::TokenType::dbusProbe:
        {
            // the lexer sets it for every D-Bus probe
            if (!node.token.dbus)
            {
                return false;
            }
            // we can match any (string, variant) property. (string, string)
            // does a regex
            bool foundProbe = false;
            return probeDbus(node.token.dbus->interface,
                             node.token.dbus->properties, foundDevs,
                             dbusProbeObjects, foundProbe);
        }
        case probe::TokenType::opAnd:
        case probe::TokenType::opOr:
        {
            const bool left = evaluateNode(*node.left, dbusProbeObjects,
                                           passedProbes, foundDevs);
            const bool right = evaluateNode(*node.right, dbusProbeObjects,
                                            passedProbes, foundDevs);
            return node.token.type == probe::TokenType::opAnd
                       ? left && right
                       : left || right;
        }
        case probe::TokenType::matchOne:
        {
            // a flag of the tree, never a node
            break;
        }
    }
    return false;
}

// default probe entry point, evaluates the tree of a probe
bool probe::detail::doProbe(
    const probe::Ast& ast, const MapperGetSubTreeResponse& dbusProbeObjects,
    const std::vector<std::string>& passedProbes, scan::FoundDevices& foundDevs)
{
    const bool ret = ast.root && evaluateNode(*ast.root, dbusProbeObjects,
                                              passedProbes, foundDevs);

    // probe passed, but empty device
    if (ret && foundDevs.empty())
    {
        foundDevs.emplace_back(
            std::flat_map<std::string, DBusValueVariant, std::less<>>{},
            std::string{});
    }
    if (ast.matchOne && ret)
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

PerformProbe::PerformProbe(EMConfig& recordRef, std::string probeName,
                           std::shared_ptr<scan::PerformScan>& scanPtr) :
    recordRef(recordRef), probeName(std::move(probeName)), scan(scanPtr)
{}

PerformProbe::~PerformProbe()
{
    scan::FoundDevices foundDevs;
    if (detail::doProbe(*recordRef.probeAst, scan->dbusProbeObjects,
                        scan->passedProbes, foundDevs))
    {
        scan->updateSystemConfiguration(recordRef, probeName, foundDevs);
    }
}

} // namespace probe
