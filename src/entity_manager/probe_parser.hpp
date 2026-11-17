#pragma once

#include "probe_lexer.hpp"

#include <string>
#include <vector>

namespace probe
{

// Parse validated probe statements into a token stream. The statements are
// joined with single spaces and lexed. Returns an empty vector on a lexing
// error; a valid probe is never empty.
std::vector<Token> parseProbeCommand(
    const std::vector<std::string>& probeField);

} // namespace probe
