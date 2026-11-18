#pragma once

#include "probe_lexer.hpp"
#include "probe_parser.hpp"

#include <nlohmann/json.hpp>

#include <flat_map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class EMConfig
{
  public:
    explicit EMConfig() = default;

    static std::optional<EMConfig> fromJson(const nlohmann::json& config);

    nlohmann::json toJson() const;
    nlohmann::json::object_t toJsonObject() const;

    std::string name;
    std::string type;

    bool isArrayProbeStmt = true;
    std::vector<std::string> probeStmt;
    // The tokens of probeStmt, set by fromJson, which rejects a configuration
    // whose Probe does not parse. The D-Bus interfaces to probe are read from
    // them.
    std::vector<probe::Token> probeTokens;
    // The tree of probeStmt, set by fromJson with the tokens and evaluated by
    // doProbe. The copies of this record that the scan makes share it.
    std::shared_ptr<const probe::Ast> probeAst;

    std::vector<nlohmann::json::object_t> exposesRecords;

    // map of extra interfaces starting with 'xyz.openbmc_project....'
    std::flat_map<std::string, nlohmann::json::object_t> extraInterfaces;

  private:
    // @throws exception on error, prefer 'fromJson' function
    explicit EMConfig(const nlohmann::json::object_t& config);
};
