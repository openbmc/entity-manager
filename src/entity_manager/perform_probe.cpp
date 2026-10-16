// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "perform_probe.hpp"

#include "perform_scan.hpp"
#include "probe_lexer.hpp"
#include "probe_parser.hpp"

#include <phosphor-logging/lg2.hpp>

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
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

// Evaluate a single leaf operand (TRUE / FALSE / FOUND / dbusProbe). On a
// malformed D-Bus statement, sets 'error' and returns false; the caller then
// fails the whole probe, matching the historic behavior.
static bool evalLeaf(const probe::AstNode& node,
                     const std::shared_ptr<scan::PerformScan>& scan,
                     scan::FoundDevices& foundDevs, bool& error)
{
    // Once a leaf has failed to parse, later leaves need no D-Bus work: the
    // whole probe is already doomed. The fold still visits them, but cheaply.
    if (error)
    {
        return false;
    }
    switch (node.leafType)
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
            return std::ranges::contains(scan->passedProbes, node.leafValue);
        }
        case probe::TokenType::dbusProbe:
        {
            // leafValue is the full "iface({...})" statement.
            const std::string& stmt = node.leafValue;
            size_t open = stmt.find('(');
            size_t close = stmt.rfind(')');
            if (open == std::string::npos || close == std::string::npos ||
                close < open)
            {
                lg2::error("dbus probe syntax error {JSON}", "JSON", stmt);
                error = true;
                return false;
            }
            std::string interface = stmt.substr(0, open);
            std::string commandStr = stmt.substr(open + 1, close - open - 1);
            // convert single ticks and single slashes into legal json
            std::ranges::replace(commandStr, '\'', '"');
            replaceAll(commandStr, R"(\)", R"(\\)");
            auto json = nlohmann::json::parse(commandStr, nullptr, false, true);
            if (json.is_discarded())
            {
                lg2::error("dbus command syntax error {STR}", "STR",
                           commandStr);
                error = true;
                return false;
            }
            // we can match any (string, variant) property. (string, string)
            // does a regex
            std::map<std::string, nlohmann::json> dbusProbeMap =
                json.get<std::map<std::string, nlohmann::json>>();
            bool foundProbe = false;
            return probeDbus(interface, dbusProbeMap, foundDevs, scan,
                             foundProbe);
        }
        default:
        {
            // Not an operand; the parser should never produce this.
            error = true;
            return false;
        }
    }
}

// Reconstruct a human-readable probe string from its tokens, for diagnostics.
static std::string describeProbe(const std::vector<probe::Token>& tokens)
{
    std::string out;
    for (const probe::Token& token : tokens)
    {
        if (!out.empty())
        {
            out += ' ';
        }
        switch (token.type)
        {
            case probe::TokenType::boolTrue:
                out += "TRUE";
                break;
            case probe::TokenType::boolFalse:
                out += "FALSE";
                break;
            case probe::TokenType::opAnd:
                out += "AND";
                break;
            case probe::TokenType::opOr:
                out += "OR";
                break;
            case probe::TokenType::matchOne:
                out += "MATCH_ONE";
                break;
            case probe::TokenType::lparen:
                out += "(";
                break;
            case probe::TokenType::rparen:
                out += ")";
                break;
            case probe::TokenType::found:
                out += "FOUND('" + token.value + "')";
                break;
            case probe::TokenType::dbusProbe:
                out += token.value;
                break;
        }
    }
    return out;
}

// default probe entry point: parse the token stream into an AST, then evaluate
// it against the scanned D-Bus objects.
bool doProbe(const std::vector<probe::Token>& probeCommand,
             const std::shared_ptr<scan::PerformScan>& scan,
             scan::FoundDevices& foundDevs)
{
    std::optional<probe::ProbeAst> ast = probe::parseProbe(probeCommand);
    if (!ast)
    {
        lg2::error("probe grammar error: {PROBE}", "PROBE",
                   describeProbe(probeCommand));
        return false;
    }

    bool error = false;
    bool ret = probe::evalAst(ast->root, [&](const probe::AstNode& leaf) {
        return evalLeaf(leaf, scan, foundDevs, error);
    });
    if (error)
    {
        return false;
    }

    // probe passed, but empty device
    if (ret && foundDevs.empty())
    {
        foundDevs.emplace_back(
            std::flat_map<std::string, DBusValueVariant, std::less<>>{},
            std::string{});
    }
    if (ast->matchOne && ret)
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
