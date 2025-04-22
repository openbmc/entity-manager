#include "config_type_tree.hpp"

#include <map>

namespace config_type_tree
{

/*
const ConfigTypeNode firmwareNode = {
    "",
    {
        {"FirmwareInfo", {"FirmwareInfo", {}}},
        {"MuxOutputs", {"MuxOutput", {}}},
    }};
    */

// maps "Type" of toplevel configuration to map which tells us the DBus
// interface name suffix for the nested object properties. e.g. "Type": "ADC" ->
// { {"Thresholds", "Threshold"} }
const std::map<std::string, ConfigTypeNode> configTypeTree = {
    {
        //{"XDPE1X2XXFirmware", firmwareNode},
        //{"ISL69269Firmware", firmwareNode},
    },
};

bool hasConfigTypeNode(const std::string& type)
{
    return configTypeTree.contains(type);
}

ConfigTypeNode getConfigTypeNode(const std::string& type)
{
    // Construct a new node to remember the 'type'.
    // The rest of the properties can be shared.
    return ConfigTypeNode(type, configTypeTree.at(type).properties);
}

}; // namespace config_type_tree
