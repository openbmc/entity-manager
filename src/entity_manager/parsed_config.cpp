#include "parsed_config.hpp"

#include "em_config.hpp"
#include "probe_lexer.hpp"

#include <phosphor-logging/lg2.hpp>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

ParsedConfig::ParsedConfig(EMConfig config, std::vector<probe::Token> tokens) :
    _config(std::move(config)),
    _tokens(
        std::make_shared<const std::vector<probe::Token>>(std::move(tokens)))
{}

std::optional<ParsedConfig> ParsedConfig::make(EMConfig config)
{
    std::vector<probe::Token> tokens =
        probe::parseProbeCommand(config.probeStmt);
    if (tokens.empty())
    {
        lg2::error("Dropping configuration {NAME}: invalid Probe statement",
                   "NAME", config.name);
        return std::nullopt;
    }
    return ParsedConfig(std::move(config), std::move(tokens));
}
