#include "dbus_probe.hpp"

DbusProbe::DbusProbe(std::string* name, nlohmann::json* command,
    nlohmann::json& ref, std::set<std::string>* interface) :
  name(*name),
  command(*command),
  ref(&ref), interface(*interface)
{}
