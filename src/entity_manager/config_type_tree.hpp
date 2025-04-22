#pragma once

// Starting from `Type` field of toplevel configuration,
// get the `Type` field for nested objects to name DBus interfaces.

#include <map>
#include <string>

namespace config_type_tree
{

class ConfigTypeNode
{
  public:
    // suffix for DBus interface
    const std::string type;

    // nested properties
    // e.g. "Thresholds" -> {"", "Threshold", {}}
    const std::map<std::string, ConfigTypeNode> properties;
};

bool hasConfigTypeNode(const std::string& type);
ConfigTypeNode getConfigTypeNode(const std::string& type);

}; // namespace config_type_tree
