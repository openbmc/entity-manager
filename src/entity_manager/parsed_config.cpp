#include "parsed_config.hpp"

#include "em_config.hpp"
#include "perform_scan.hpp"
#include "probe_lexer.hpp"

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
        scan::detail::parseProbeCommand(config.probeStmt);
    if (tokens.empty())
    {
        return std::nullopt;
    }
    return ParsedConfig(std::move(config), std::move(tokens));
}
