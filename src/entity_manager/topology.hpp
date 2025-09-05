#pragma once

#include <nlohmann/json.hpp>

#include <set>
#include <unordered_map>

using Association = std::tuple<std::string, std::string, std::string>;

using BoardPathsView = decltype(std::views::keys(
    std::declval<std::map<std::string, std::string>&>()));

class AssocName
{
  public:
    std::string name;
    std::string reverse;

    // on which board type (e.g. Chassis, Board, Valve, ...) can we have this
    // association
    std::set<std::string> allowedOnBoardTypes;

    AssocName getReverse() const;

    bool operator==(const AssocName& other) const = default;
    bool operator<(const AssocName& other) const;
};

extern const std::vector<AssocName> assocs;

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

    // @brief: fill associations map with the associations for a port identifier
    // such as 'MB Upstream Port'
    void fillAssocsForPortId(
        std::unordered_map<std::string, std::set<Association>>& result,
        BoardPathsView boardPaths,
        const std::map<Path, std::set<AssocName>>& pathAssocs);

    void fillAssocForPortId(
        std::unordered_map<std::string, std::set<Association>>& result,
        BoardPathsView boardPaths, const Path& upstream, const Path& downstream,
        const AssocName& assocName);

    void addConfiguredPort(const Path& path, const nlohmann::json& exposesItem);
    void addPort(const PortType& port, const Path& path,
                 const AssocName& assocName);

    static std::optional<AssocName> getAssocByName(const std::string& name);

    // Maps the port name to the participating paths.
    // each path also has their role(s) in the association.
    // For example a PSU path which is part of "MB Upstream Port"
    // will have "powering" role.
    std::unordered_map<PortType, std::map<Path, std::set<AssocName>>> ports;

    std::unordered_map<Path, BoardType> boardTypes;
    std::unordered_map<BoardName, Path> boardNames;
};
