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

    downstreamPorts[connectsTo].insert(path);
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
    upstreamPorts[exposesType].insert(path);
    boardTypes[path] = boardType;
}

std::unordered_map<std::string, std::set<Association>> Topology::getAssocs(
    BoardPathsView boardPaths)
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

        fillAssocsForPortId(result, boardPaths, upstreamPortPair.second,
                            downstreamMatch->second);
    }
    return result;
}

void Topology::fillAssocsForPortId(
    std::unordered_map<std::string, std::set<Association>>& result,
    BoardPathsView boardPaths, const std::set<Path>& upstreamPaths,
    const std::set<Path>& downstreamPaths)
{
    for (const Path& upstream : upstreamPaths)
    {
        for (const Path& downstream : downstreamPaths)
        {
            fillAssocForPortId(result, boardPaths, upstream, downstream);
        }
    }
}

void Topology::fillAssocForPortId(
    std::unordered_map<std::string, std::set<Association>>& result,
    BoardPathsView boardPaths, const Path& upstream, const Path& downstream)
{
    if (boardTypes[upstream] != "Chassis" && boardTypes[upstream] != "Board")
    {
        return;
    }
    // The downstream path must be one we care about.
    if (!std::ranges::contains(boardPaths, downstream))
    {
        return;
    }

    std::string assoc = "contained_by";

    result[downstream].insert(
        {assoc, getReverseAssoc(assoc).value(), upstream});

    if (powerPaths.contains(downstream))
    {
        assoc = "powering";
        result[downstream].insert(
            {assoc, getReverseAssoc(assoc).value(), upstream});
    }
}

const std::set<std::pair<std::string, std::string>> assocs = {
    {"powering", "powered_by"}, {"containing", "contained_by"},
    // ... extend as needed
};

std::optional<std::string> Topology::getReverseAssoc(const AssocName& assocName)
{
    for (const auto& entry : assocs)
    {
        if (entry.first == assocName)
        {
            return entry.second;
        }
        if (entry.second == assocName)
        {
            return entry.first;
        }
    }

    return std::nullopt;
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
        upstreamPort.second.erase(boardPath);
    }

    for (auto& downstreamPort : downstreamPorts)
    {
        downstreamPort.second.erase(boardPath);
    }
}
