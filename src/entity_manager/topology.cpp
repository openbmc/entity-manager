#include "topology.hpp"

#include "phosphor-logging/lg2.hpp"

const AssocName assocContaining =
    AssocName("containing", "contained_by", {"Chassis"},
              {"Board", "Chassis", "PowerSupply"});
const AssocName assocContainedBy = assocContaining.getReverse();

// Topology tests say that a chassis can be powering another chassis.
// In case there is any confusion as to why 'Chassis' can have 'powering'
// association.
const AssocName assocPowering =
    AssocName("powering", "powered_by", {"Chassis", "PowerSupply"},
              {"Board", "Chassis", "PowerSupply"});
const AssocName assocPoweredBy = assocPowering.getReverse();

const AssocName assocProbing = AssocName("probing", "probed_by", {}, {});
const AssocName assocProbedBy = assocProbing.getReverse();

const std::vector<AssocName> supportedAssocs = {
    assocContaining,
    assocContainedBy,
    assocPowering,
    assocPoweredBy,
};

AssocName::AssocName(const std::string& name, const std::string& reverse,
                     const std::set<std::string>& allowedOnBoardTypes,
                     const std::set<std::string>& allowedOnBoardTypesReverse) :
    name(name), reverse(reverse), allowedOnBoardTypes(allowedOnBoardTypes),
    allowedOnBoardTypesReverse(allowedOnBoardTypesReverse)
{}

AssocName AssocName::getReverse() const
{
    return {reverse, name, allowedOnBoardTypesReverse, allowedOnBoardTypes};
}

bool AssocName::operator<(const AssocName& other) const
{
    return name < other.name;
}

std::optional<AssocName> Topology::getAssocByName(const std::string& name)
{
    for (const auto& assoc : supportedAssocs)
    {
        if (assoc.name == name)
        {
            return assoc;
        }
    }
    return std::nullopt;
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
    else if (exposesType == "Port")
    {
        addConfiguredPort(path, exposesItem);
    }
    else if (exposesType.ends_with("Port"))
    {
        addPort(exposesType, path, assocContaining);

        // this represents the legacy quirk of upstream ports having no choice
        // in the
        // powered_by association
        addPort(exposesType, path, assocPoweredBy);
    }
    else
    {
        return;
    }
    boardTypes[path] = boardType;
}

void Topology::addConfiguredPort(const Path& path,
                                 const nlohmann::json& exposesItem)
{
    const auto findConnectsToName = exposesItem.find("Name");
    if (findConnectsToName == exposesItem.end())
    {
        lg2::error("Board at path {PATH} is missing 'Name'", "PATH", path);
        return;
    }
    const std::string connectsToName = findConnectsToName->get<std::string>();

    const auto findPortType = exposesItem.find("PortType");
    if (findPortType == exposesItem.end())
    {
        lg2::error("Board at path {PATH} is missing PortType", "PATH", path);
        return;
    }
    const std::string portType = findPortType->get<std::string>();

    const auto assoc = getAssocByName(portType);
    if (!assoc.has_value())
    {
        lg2::error("Could not find configured association name {ASSOC}",
                   "ASSOC", portType);
        return;
    }

    addPort(connectsToName, path, assoc.value());
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

    for (const auto& [boardPath, probePaths] : probePaths)
    {
        if (std::ranges::contains(boardPaths, boardPath))
        {
            for (const auto& path : probePaths)
            {
                result[boardPath].insert(
                    {assocProbing.name, assocProbedBy.name, path});
            }
        }
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
                // if the other end of the association does not declare
                // the reverse association, do not associate
                const bool otherAgrees =
                    other.second.contains(assocName.getReverse());

                if (!otherAgrees)
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
    if (!assocName.allowedOnBoardTypes.contains(boardTypes[upstream]))
    {
        lg2::error(
            "Cannot create Association Definition {ASSOC} for {PATH} with board type {TYPE}",
            "ASSOC", assocName.name, "PATH", upstream, "TYPE",
            boardTypes[upstream]);
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

    probePaths.erase(boardName);
}

void Topology::addProbePath(const std::string& boardPath,
                            const std::string& probePath)
{
    lg2::info("Probe path added: {PROBE} probed_by {BOARD} boards config",
              "PROBE", probePath, "BOARD", boardPath);
    probePaths[boardPath].emplace(probePath);
}
