#pragma once

#include <nlohmann/json.hpp>

#include <set>
#include <unordered_map>

using Association = std::tuple<std::string, std::string, std::string>;

using BoardPathsView = decltype(std::views::keys(
    std::declval<std::map<std::string, std::string>&>()));

class Topology
{
  public:
    explicit Topology() = default;

    void addBoard(const std::string& path, const std::string& boardType,
                  const std::string& boardName,
                  const nlohmann::json& exposesItem);
    std::unordered_map<std::string, std::set<Association>> getAssocs(
        BoardPathsView boardPaths);
    void remove(const std::string& boardName);

  private:
    using Path = std::string;
    using BoardType = std::string;
    using BoardName = std::string;
    using PortType = std::string;

    void addDownstreamPort(const Path& path, const nlohmann::json& exposesItem);

    // e.g. contained_by, containing, powered_by, ...
    using AssocName = std::string;

    // @brief: fill associations map with the associations for a port identifier
    // such as 'MB Upstream Port'
    void fillAssocsForPortId(
        std::unordered_map<std::string, std::set<Association>>& result,
        BoardPathsView boardPaths, const std::set<Path>& upstreamPaths,
        const std::set<Path>& downstreamPaths);

    void fillAssocForPortId(
        std::unordered_map<std::string, std::set<Association>>& result,
        BoardPathsView boardPaths, const Path& upstream,
        const Path& downstream);

    static std::optional<std::string> getOppositeAssoc(
        const AssocName& assocName);

    std::unordered_map<PortType, std::set<Path>> upstreamPorts;
    std::unordered_map<PortType, std::set<Path>> downstreamPorts;
    std::set<Path> powerPaths;
    std::set<Path> fanPaths;
    std::unordered_map<Path, BoardType> boardTypes;
    std::unordered_map<BoardName, Path> boardNames;
};
