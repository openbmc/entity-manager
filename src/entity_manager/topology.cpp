#include "topology.hpp"

#include "phosphor-logging/lg2.hpp"

const AssocName assocContaining = AssocName("containing", "contained_by");
const AssocName assocContainedBy = assocContaining.getReverse();
const AssocName assocPowering = AssocName("powering", "powered_by");
const AssocName assocPoweredBy = assocPowering.getReverse();

AssocName::AssocName(const std::string& name, const std::string& reverse) :
    name(name), reverse(reverse)
{}

AssocName AssocName::getReverse() const
{
    return {reverse, name};
}

bool AssocName::operator<(const AssocName& other) const
{
    return name < other.name;
}

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
        addPort(exposesType, path, assocContaining);
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

    addPort(connectsTo, path, assocContainedBy);

    auto findPoweredBy = exposesItem.find("PowerPort");
    if (findPoweredBy != exposesItem.end())
    {
        addPort(connectsTo, path, assocPowering);
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
                // if the other end of the assocation does not declare
                // the reverse association, do not associate
                const bool otherAgrees =
                    other.second.contains(assocName.getReverse());

                // quirk: since the other side of the association cannot declare
                // to be powered_by in the legacy schema, in case of "powering",
                // the two associations do not have to agree.
                if (!otherAgrees && assocName != assocPowering)
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

    // quirk: legacy code did not associate from both sides
    // TODO(alexander): revisit this
    if (assocName == assocContaining || assocName == assocPoweredBy)
    {
        return;
    }

    result[upstream].insert({assocName.name, assocName.reverse, downstream});
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
