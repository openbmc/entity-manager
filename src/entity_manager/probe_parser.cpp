#include "probe_parser.hpp"

#include "probe_lexer.hpp"

#include <phosphor-logging/lg2.hpp>

#include <optional>
#include <string>
#include <vector>

namespace probe
{

std::vector<Token> parseProbeCommand(const std::vector<std::string>& probeField)
{
    // Join the statements (array form) into a single string, then lex it.
    std::string joined;
    for (const auto& probe : probeField)
    {
        if (!joined.empty())
        {
            joined += ' ';
        }
        joined += probe;
    }

    std::optional<std::vector<Token>> tokens = lexProbe(joined);
    if (!tokens)
    {
        lg2::error("Probe statement failed to parse: {PROBE}", "PROBE", joined);
        return {};
    }
    if (tokens->empty())
    {
        lg2::error("Probe statement produced no tokens: {PROBE}", "PROBE",
                   joined);
    }
    return *tokens;
}

} // namespace probe
