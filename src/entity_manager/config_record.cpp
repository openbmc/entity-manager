#include "config_record.hpp"

ConfigRecord::ConfigRecord(std::string& name, nlohmann::json& probeStmt,
                           nlohmann::json& ref,
                           std::set<std::string>& probeInterfaces) :
    name(name), probeStatement(probeStmt), ref(&ref),
    probeInterfaces(probeInterfaces)
{}
