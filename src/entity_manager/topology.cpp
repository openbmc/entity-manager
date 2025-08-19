#include "topology.hpp"

#include "phosphor-logging/lg2.hpp"

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
        addDownstreamPort(path, exposesItem);
    }
    else if (exposesType.ends_with("Port"))
    {
        upstreamPorts[exposesType].emplace_back(path);
    }
    else
    {
        return;
    }
    boardTypes[path] = boardType;
}

void Topology::addDownstreamPort(const Path& path,
                                 const nlohmann::json& exposesItem)
{
    auto findConnectsTo = exposesItem.find("ConnectsToType");
    if (findConnectsTo == exposesItem.end())
    {
        lg2::error("Board at path {PATH} is missing ConnectsToType", "PATH",
                   path);
        return;
    }
    PortType connectsTo = findConnectsTo->get<std::string>();

    downstreamPorts[connectsTo].emplace_back(path);
    auto findPoweredBy = exposesItem.find("PowerPort");
    if (findPoweredBy != exposesItem.end())
    {
        powerPaths.insert(path);
    }
}

std::unordered_map<std::string, std::set<Association>> Topology::getAssocs(
    const std::map<Path, BoardName>& boards)
{
    std::unordered_map<std::string, std::set<Association>> result;

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
                    if (boards.contains(downstream))
                    {
                        result[downstream].insert(
                            {"contained_by", "containing", upstream});
                        if (powerPaths.contains(downstream))
                        {
                            result[downstream].insert(
                                {"powering", "powered_by", upstream});
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

    for (auto& upstreamPort : upstreamPorts)
    {
        std::erase(upstreamPort.second, boardPath);
    }

    for (auto& downstreamPort : downstreamPorts)
    {
        std::erase(downstreamPort.second, boardPath);
    }
}
