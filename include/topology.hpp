#pragma once

#include <nlohmann/json.hpp>

#include <set>
#include <unordered_map>

using Association = std::tuple<std::string, std::string, std::string>;

class Topology
{
  public:
    explicit Topology() = default;

    void addBoard(const std::string& path, const std::string& boardType,
                  const nlohmann::json& exposesItem);
    std::unordered_map<std::string, std::vector<Association>> getAssocs();

  private:
    using Path = std::string;
    using BoardType = std::string;
    using PortType = std::string;

    std::unordered_map<PortType, std::vector<Path>> upstreamPorts;
    std::unordered_map<PortType, std::vector<Path>> downstreamPorts;
    std::unordered_map<Path, BoardType> boardTypes;
};
