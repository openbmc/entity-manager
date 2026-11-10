#pragma once

#include "em_config.hpp"
#include "probe_lexer.hpp"

#include <memory>
#include <optional>
#include <vector>

// An EMConfig with the tokens of its Probe, which parsed. The tokens describe
// the Probe as loaded, so a copy of config() with resolved templates does not
// carry them.
class ParsedConfig
{
  public:
    // Returns std::nullopt if the Probe is empty or does not parse.
    static std::optional<ParsedConfig> make(EMConfig config);

    const EMConfig& config() const
    {
        return _config;
    }

    const std::vector<probe::Token>& tokens() const
    {
        return *_tokens;
    }

  private:
    ParsedConfig(EMConfig config, std::vector<probe::Token> tokens);

    EMConfig _config;
    // Shared so that copying the configurations for every scan does not copy
    // the tokens.
    std::shared_ptr<const std::vector<probe::Token>> _tokens;
};
