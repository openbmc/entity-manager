#include "topology.hpp"

#include "phosphor-logging/lg2.hpp"

PHOSPHOR_LOG2_USING;

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
        upstreamPorts[exposesType].insert(path);
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

    downstreamPorts[connectsTo].insert(path);
    auto findPoweredBy = exposesItem.find("PowerPort");
    if (findPoweredBy != exposesItem.end())
    {
        powerPaths.insert(path);
    }
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

    for (const auto& [boardPath, paths] : probePaths)
    {
        for (const auto& probePath : paths)
        {
            std::string probeAssocName = "probed_by";
            std::optional<std::string> opposite =
                getOppositeAssoc(probeAssocName);
            if (!opposite.has_value())
            {
                continue;
            }

            result[boardPath].insert(
                {probeAssocName, opposite.value(), probePath});
        }
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
    std::optional<std::string> opposite = getOppositeAssoc(assoc);

    if (!opposite.has_value())
    {
        return;
    }

    result[downstream].insert({assoc, opposite.value(), upstream});

    if (powerPaths.contains(downstream))
    {
        assoc = "powering";
        opposite = getOppositeAssoc(assoc);
        if (!opposite.has_value())
        {
            return;
        }

        result[downstream].insert({assoc, opposite.value(), upstream});
    }
}

const std::set<std::pair<std::string, std::string>> assocs = {
    {"powering", "powered_by"},
    {"containing", "contained_by"},
    {"probing", "probed_by"},
    // ... extend as needed
};

std::optional<std::string> Topology::getOppositeAssoc(
    const AssocName& assocName)
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

    probePaths.erase(boardName);
}

void Topology::addProbePath(const std::string& boardPath,
                            const std::string& probePath)
{
    info("Probe path added: {PROBE} probed_by {BOARD} boards config", "PROBE",
         probePath, "BOARD", boardPath);
    probePaths[boardPath].emplace(probePath);
}
