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
                  const std::string& boardName,
                  const nlohmann::json& exposesItem);
    std::unordered_map<std::string, std::vector<Association>> getAssocs(
        const std::map<std::string, std::string>& boards);

    // Adds an entry to probePaths for object mapper board path
    // and inventory board path.
    void addProbePath(const std::string& boardPath,
                      const std::string& probePath);
    void remove(const std::string& boardName);

  private:
    using Path = std::string;
    using BoardType = std::string;
    using BoardName = std::string;
    using PortType = std::string;

    std::unordered_map<PortType, std::vector<Path>> upstreamPorts;
    std::unordered_map<PortType, std::vector<Path>> downstreamPorts;
    std::set<Path> powerPaths;
    std::unordered_map<Path, BoardType> boardTypes;
    std::unordered_map<BoardName, Path> boardNames;

    // Represents the mapping between inventory object pathes of a
    // probed configuration and the object paths of DBus interfaces
    // it was probed on.
    std::unordered_map<Path, std::set<Path>> probePaths;
};
