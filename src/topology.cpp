#include "topology.hpp"

#include <iostream>

void Topology::addBoard(const std::string& path, const std::string& boardType,
                        const nlohmann::json& exposesItem)
{
    auto findType = exposesItem.find("Type");
    if (findType == exposesItem.end())
    {
        return;
    }
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
    }
    else if (exposesType.ends_with("Port"))
    {
        upstreamPorts[exposesType].emplace_back(path);
        boardTypes[path] = boardType;
    }
}

std::unordered_map<std::string, std::vector<Association>> Topology::getAssocs()
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
                    // Chassis containing/contained_by relationships should only
                    // be established when they are Chassis/Board <->
                    // Chassis/Board relationships
                    if (boardTypes[downstream] == "Chassis" ||
                        boardTypes[downstream] == "Board")
                    {
                        result[downstream].emplace_back("contained_by",
                                                        "containing", upstream);
                        continue;
                    }

                    // Chassis/Board <-> Component relationships are assembly
                    // associations
                    if (boardTypes[downstream] == "Component")
                    {
                        result[downstream].emplace_back("chassis", "assembly",
                                                        upstream);
                        continue;
                    }
                }
            }
        }
    }

    return result;
}
