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
        addDownstreamPort(path, boardType, exposesItem);
    }
    else if (exposesType.ends_with("Port"))
    {
        addUpstreamPort(path, boardType, exposesType);
    }
}

void Topology::addDownstreamPort(const Path& path, const BoardType& boardType,
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
    boardTypes[path] = boardType;
    auto findPoweredBy = exposesItem.find("PowerPort");
    if (findPoweredBy != exposesItem.end())
    {
        powerPaths.insert(path);
    }
}

void Topology::addUpstreamPort(const Path& path, const BoardType& boardType,
                               const PortType& exposesType)
{
    upstreamPorts[exposesType].emplace_back(path);
    boardTypes[path] = boardType;
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
                    if (boards.contains(downstream))
                    {
                        result[downstream].emplace_back("contained_by",
                                                        "containing", upstream);
                        if (powerPaths.contains(downstream))
                        {
                            result[downstream].emplace_back(
                                "powering", "powered_by", upstream);
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
