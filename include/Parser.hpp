
#pragma once
#include <iostream>
#include <optional>
#include <string>
#include <vector>

std::optional<std::string>
    parseAndEvaluate(std::string&,
                     std::vector<std::pair<std::string, std::string>>&,
                     std::vector<std::pair<std::string, int64_t>>&);
