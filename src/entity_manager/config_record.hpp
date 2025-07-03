#pragma once

#include <nlohmann/json.hpp>

#include <set>
#include <string>

class ConfigRecord
{
  public:
    ConfigRecord(std::string* name, nlohmann::json* command,
                 nlohmann::json& ref, std::set<std::string>* interface);

    std::string name;
    nlohmann::json command;
    nlohmann::json* ref;
    std::set<std::string> interface;
};
