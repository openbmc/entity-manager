#pragma once

#include <nlohmann/json.hpp>

#include <set>
#include <string>

// ConfigRecord represents information about a configuration file to pass
// around. Later it can serve to merge with information from the object
// mapper. It will help to valitedate probe statements and allows access
// to a configurtion for interface construction(ToDo!)

class ConfigRecord
{
  public:
    ConfigRecord(std::string& name, nlohmann::json& probeStmt,
                 nlohmann::json& ref, std::set<std::string>& probeInterfaces);

    // "Name" field from probe meta data of a configuration.
    std::string name;

    // "Probe" field of a configuration.
    nlohmann::json probeStatement;

    // Reference to the actual configuration, handled by the configuration
    // class.
    nlohmann::json* ref;

    // Set of probe interfaces extracted from a configuration file.
    std::set<std::string> probeInterfaces;
};
