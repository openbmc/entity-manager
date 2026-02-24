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

    AssocName(const std::string& name, const std::string& reverse,
              const std::set<std::string>& allowedOnBoardTypes,
              const std::set<std::string>& allowedOnBoardTypesReverse);
    AssocName() = delete;

    // The type (e.g. Chassis, Board, Valve, ...) on which the association is
    // allowed
    std::set<std::string> allowedOnBoardTypes;

    // The type (e.g. Chassis, Board, Valve, ...) on which the reverse
    // association is allowed
    std::set<std::string> allowedOnBoardTypesReverse;

    AssocName getReverse() const;

    bool operator==(const AssocName& other) const = default;
    bool operator<(const AssocName& other) const;
};

extern const std::vector<AssocName> supportedAssocs;

class Topology
{
  public:
    explicit Topology() = default;

    void addBoard(const std::string& path, const std::string& boardType,
                  const std::string& boardName,
                  const nlohmann::json& exposesItem);
    std::unordered_map<std::string, std::set<Association>> getAssocs(
        BoardPathsView boardPaths);

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

    // Represents the mapping between inventory object paths of a
    // probed configuration and the object paths of DBus interfaces
    // it was probed on.
    std::unordered_map<Path, std::set<Path>> probePaths;
};
