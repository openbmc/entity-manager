#pragma once

#include <set>
#include <unordered_map>

#include <nlohmann/json.hpp>

using Association = std::tuple<std::string, std::string, std::string>;

class Topology
{
  public:

    explicit Topology()
    {}

    void addBoard(std::string path, std::string boardType, const nlohmann::json& exposesItem);
    std::unordered_map<std::string, std::vector<Association>> getAssocs();

  private:
    using Path = std::string;
    using BoardType = std::string;
    using PortType = std::string;

    // Mapping of pairs of board types to their association names in order:
    // (upstream, downstream)
    //const std::unordered_map<std::pair<BoardType, BoardType>,
    //      std::pair<std::string, std::string>> assocTypes;

    std::unordered_map<PortType, std::vector<Path>> upstreamPorts;
    std::unordered_map<PortType, std::vector<Path>> downstreamPorts;
    std::unordered_map<Path, BoardType> boardTypes;
};
