#include "topology.hpp"

#include <iostream>

void Topology::addBoard(const std::string& path, const std::string& boardType,
                        const std::string& boardName,
                        const nlohmann::json& exposesItem)
{
    auto findType = exposesItem.find("Type");
    if (findType == exposesItem.end())
    {
        return;
    }

    boardNames.try_emplace(boardName, path);

    PortType exposesType = findType->get<std::string>();

    if (exposesType == "DownstreamPort")
    {
        auto findConnectsTo = exposesItem.find("ConnectsToType");
        if (findConnectsTo == exposesItem.end())
        {
            std::cerr << "Board at path " << path
                      << " is missing ConnectsToType" << std::endl;
            return;
        }
        PortType connectsTo = findConnectsTo->get<std::string>();

        downstreamPorts[connectsTo].emplace_back(path);
        boardTypes[path] = boardType;
        auto findPoweredBy = exposesItem.find("PowerPort");
        if (findPoweredBy != exposesItem.end())
        {
            powerPaths.insert(path);
        }
    }
    else if (exposesType.ends_with("Port"))
    {
        upstreamPorts[exposesType].emplace_back(path);
        boardTypes[path] = boardType;
    }
}

std::unordered_map<std::string, std::vector<Association>> Topology::getAssocs(
    const std::map<Path, BoardName>& boards)
{
    std::unordered_map<std::string, std::vector<Association>> result;

    // look at each upstream port type
    for (const auto& upstreamPortPair : upstreamPorts)
    {
        auto downstreamMatch = downstreamPorts.find(upstreamPortPair.first);

        if (downstreamMatch == downstreamPorts.end())
        {
            // no match
            continue;
        }

        for (const Path& upstream : upstreamPortPair.second)
        {
            if (boardTypes[upstream] == "Chassis" ||
                boardTypes[upstream] == "Board")
            {
                for (const Path& downstream : downstreamMatch->second)
                {
                    // The downstream path must be one we care about.
                    if (boards.find(downstream) != boards.end())
                    {
                        result[downstream].emplace_back("contained_by",
                                                        "containing", upstream);
                        if (powerPaths.find(downstream) != powerPaths.end())
                        {
                            result[upstream].emplace_back(
                                "powered_by", "powering", downstream);
                        }
                    }
                }
            }
        }
    }

    return result;
}

void Topology::remove(const std::string& boardName)
{
    // Remove the board from boardNames, and then using the path
    // found in boardNames remove it from upstreamPorts and
    // downstreamPorts.
    auto boardFind = boardNames.find(boardName);
    if (boardFind == boardNames.end())
    {
        return;
    }

    std::string boardPath = boardFind->second;

    boardNames.erase(boardFind);

    for (auto it = upstreamPorts.begin(); it != upstreamPorts.end();)
    {
        auto pathIt =
            std::find(it->second.begin(), it->second.end(), boardPath);
        if (pathIt != it->second.end())
        {
            it->second.erase(pathIt);
        }

        if (it->second.empty())
        {
            it = upstreamPorts.erase(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto it = downstreamPorts.begin(); it != downstreamPorts.end();)
    {
        auto pathIt =
            std::find(it->second.begin(), it->second.end(), boardPath);
        if (pathIt != it->second.end())
        {
            it->second.erase(pathIt);
        }

        if (it->second.empty())
        {
            it = downstreamPorts.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
