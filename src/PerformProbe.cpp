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
/// \file PerformProbe.cpp
#include "EntityManager.hpp"

#include <boost/algorithm/string/replace.hpp>

#include <regex>

constexpr const bool debug = false;

// probes dbus interface dictionary for a key with a value that matches a regex
// When an interface passes a probe, also save its D-Bus path with it.
bool probeDbus(const std::string& interface,
               const std::map<std::string, nlohmann::json>& matches,
               FoundDeviceT& devices, const std::shared_ptr<PerformScan>& scan,
               bool& foundProbe)
{
    bool foundMatch = false;
    foundProbe = false;

    for (const auto& [path, interfaces] : scan->dbusProbeObjects)
    {
        auto it = interfaces.find(interface);
        if (it == interfaces.end())
        {
            continue;
        }

        foundProbe = true;

        bool deviceMatches = true;
        const boost::container::flat_map<std::string, DBusValueVariant>&
            properties = it->second;

        for (const auto& [matchProp, matchJSON] : matches)
        {
            auto deviceValue = properties.find(matchProp);
            if (deviceValue != properties.end())
            {
                deviceMatches =
                    deviceMatches && matchProbe(matchJSON, deviceValue->second);
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
            if constexpr (debug)
            {
                std::cerr << "probeDBus: Found probe match on " << path << " "
                          << interface << "\n";
            }
            devices.emplace_back(properties, path);
            foundMatch = true;
        }
    }
    return foundMatch;
}

// default probe entry point, iterates a list looking for specific types to
// call specific probe functions
bool probe(const std::vector<std::string>& probeCommand,
           const std::shared_ptr<PerformScan>& scan, FoundDeviceT& foundDevs)
{
    const static std::regex command(R"(\((.*)\))");
    std::smatch match;
    bool ret = false;
    bool matchOne = false;
    bool cur = true;
    probe_type_codes lastCommand = probe_type_codes::FALSE_T;
    bool first = true;

    for (auto& probe : probeCommand)
    {
        FoundProbeTypeT probeType = findProbeType(probe);
        if (probeType)
        {
            switch ((*probeType)->second)
            {
                case probe_type_codes::FALSE_T:
                {
                    cur = false;
                    break;
                }
                case probe_type_codes::TRUE_T:
                {
                    cur = true;
                    break;
                }
                case probe_type_codes::MATCH_ONE:
                {
                    // set current value to last, this probe type shouldn't
                    // affect the outcome
                    cur = ret;
                    matchOne = true;
                    break;
                }
                /*case probe_type_codes::AND:
                  break;
                case probe_type_codes::OR:
                  break;
                  // these are no-ops until the last command switch
                  */
                case probe_type_codes::FOUND:
                {
                    if (!std::regex_search(probe, match, command))
                    {
                        std::cerr << "found probe syntax error " << probe
                                  << "\n";
                        return false;
                    }
                    std::string commandStr = *(match.begin() + 1);
                    boost::replace_all(commandStr, "'", "");
                    cur = (std::find(scan->passedProbes.begin(),
                                     scan->passedProbes.end(),
                                     commandStr) != scan->passedProbes.end());
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
            auto json = nlohmann::json::parse(commandStr, nullptr, false);
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
            cur = probeDbus(probeInterface, dbusProbeMap, foundDevs, scan,
                            foundProbe);
        }

        // some functions like AND and OR only take affect after the
        // fact
        if (lastCommand == probe_type_codes::AND)
        {
            ret = cur && ret;
        }
        else if (lastCommand == probe_type_codes::OR)
        {
            ret = cur || ret;
        }

        if (first)
        {
            ret = cur;
            first = false;
        }
        lastCommand =
            probeType ? (*probeType)->second : probe_type_codes::FALSE_T;
    }

    // probe passed, but empty device
    if (ret && foundDevs.size() == 0)
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

PerformProbe::PerformProbe(
    const std::vector<std::string>& probeCommand,
    std::shared_ptr<PerformScan>& scanPtr,
    std::function<void(FoundDeviceT&, const DBusSubtree&)>&& callback) :
    _probeCommand(probeCommand),
    scan(scanPtr), _callback(std::move(callback))
{}
PerformProbe::~PerformProbe()
{
    FoundDeviceT foundDevs;
    if (probe(_probeCommand, scan, foundDevs))
    {
        _callback(foundDevs, scan->dbusProbeObjects);
    }
}
