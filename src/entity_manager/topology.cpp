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
        addPort(exposesType, path, "containing");
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

    addPort(connectsTo, path, "contained_by");

    auto findPoweredBy = exposesItem.find("PowerPort");
    if (findPoweredBy != exposesItem.end())
    {
        addPort(connectsTo, path, "powering");
    }
}

void Topology::addPort(const PortType& port, const Path& path,
                       const AssocName& assocName)
{
    if (!ports.contains(port))
    {
        ports.insert({port, {}});
    }
    if (!ports[port].contains(path))
    {
        ports[port].insert({path, {}});
    }
    ports[port][path].insert(assocName);
}

std::unordered_map<std::string, std::set<Association>> Topology::getAssocs(
    BoardPathsView boardPaths)
{
    std::unordered_map<std::string, std::set<Association>> result;

    // look at each upstream port type
    for (const auto& port : ports)
    {
        fillAssocsForPortId(result, boardPaths, port.second);
    }
    return result;
}

void Topology::fillAssocsForPortId(
    std::unordered_map<std::string, std::set<Association>>& result,
    BoardPathsView boardPaths,
    const std::map<Path, std::set<AssocName>>& pathAssocs)
{
    for (const auto& member : pathAssocs)
    {
        for (const auto& other : pathAssocs)
        {
            if (other.first == member.first)
            {
                continue;
            }
            for (const auto& assocName : member.second)
            {
                auto optReverse = getOppositeAssoc(assocName);
                if (!optReverse.has_value())
                {
                    continue;
                }
                // if the other end of the assocation does not declare
                // the reverse association, do not associate
                const bool otherAgrees =
                    other.second.contains(optReverse.value());

                // quirk: since the other side of the association cannot declare
                // to be powered_by in the legacy schema, in case of "powering",
                // the two associations do not have to agree.
                if (!otherAgrees && assocName != "powering")
                {
                    continue;
                }

                fillAssocForPortId(result, boardPaths, member.first,
                                   other.first, assocName);
            }
        }
    }
}

void Topology::fillAssocForPortId(
    std::unordered_map<std::string, std::set<Association>>& result,
    BoardPathsView boardPaths, const Path& upstream, const Path& downstream,
    const AssocName& assocName)
{
    if (boardTypes[upstream] != "Chassis" && boardTypes[upstream] != "Board")
    {
        return;
    }
    // The downstream path must be one we care about.
    if (!std::ranges::contains(boardPaths, upstream))
    {
        return;
    }

    auto optReverse = getOppositeAssoc(assocName);

    if (!optReverse)
    {
        return;
    }

    // quirk: legacy code did not associate from both sides
    // TODO(alexander): revisit this
    if (assocName == "containing" || assocName == "powered_by")
    {
        return;
    }

    result[upstream].insert({assocName, optReverse.value(), downstream});
}

const std::set<std::pair<std::string, std::string>> assocs = {
    {"powering", "powered_by"}, {"containing", "contained_by"},
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
    // found in boardNames remove it from ports
    auto boardFind = boardNames.find(boardName);
    if (boardFind == boardNames.end())
    {
        return;
    }

    std::string boardPath = boardFind->second;

    boardNames.erase(boardFind);

    for (auto& port : ports)
    {
        port.second.erase(boardPath);
    }
}
