#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/ManagedHost/common.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <string>

namespace managed_host
{

using HostState = sdbusplus::common::xyz::openbmc_project::state::Host;

const std::string powerBasePath = std::format(
    "{}/{}", HostState::namespace_path::value, HostState::namespace_path::host);

std::string hostPowerPath(size_t hostIndex)
{
    return std::format("{}{}", powerBasePath, hostIndex);
}

ssize_t hostIndexFromPath(const std::string& path)
{
    if (!path.starts_with(powerBasePath))
    {
        return -1;
    }
    std::string hostNum = path.substr(powerBasePath.size());
    try
    {
        size_t hostIndex = stoul(hostNum);
        return hostIndex;
    }
    catch (std::exception& e)
    {
        lg2::error(
            "could not get host index from '{STR}' of path {PATH}: {ERR}",
            "STR", hostNum, "PATH", path, "ERR", e);
    }
    return -1;
}

// non-logging
static ssize_t getManagedHostIndexInternal(
    const nlohmann::json::object_t* config)
{
    using ManagedHost = sdbusplus::common::xyz::openbmc_project::inventory::
        decorator::ManagedHost;

    auto mhFind = config->find(ManagedHost::interface);

    if (mhFind == config->end())
    {
        return -1;
    }

    const nlohmann::json::object_t* mh =
        mhFind->second.get_ptr<const nlohmann::json::object_t*>();

    auto hostIndexFind = mh->find(ManagedHost::property_names::host_index);

    if (hostIndexFind == mh->end())
    {
        return -1;
    }

    if (!hostIndexFind->second.is_number())
    {
        return -1;
    }

    return hostIndexFind->second.get<int>();
}

size_t getManagedHostIndex(const std::string& name,
                           const nlohmann::json::object_t* config)
{
    const ssize_t hostIndex = getManagedHostIndexInternal(config);

    if (hostIndex < 0)
    {
        lg2::debug("Could not find ManagedHost index in config for {NAME}",
                   "NAME", name);
        return 0;
    }

    lg2::debug("Found ManagedHost index {INDEX} in config for {NAME}", "INDEX",
               hostIndex, "NAME", name);
    return hostIndex;
}

} // namespace managed_host
