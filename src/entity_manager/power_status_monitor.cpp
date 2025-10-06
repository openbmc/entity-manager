#include "power_status_monitor.hpp"

#include "managed_host.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/ObjectMapper/client.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <flat_map>

namespace power
{

using HostState = sdbusplus::common::xyz::openbmc_project::state::Host;
using ObjectMapper = sdbusplus::common::xyz::openbmc_project::ObjectMapper;

const std::string powerBasePath = std::format(
    "{}/{}", HostState::namespace_path::value, HostState::namespace_path::host);

PowerStatusMonitor::PowerStatusMonitor(sdbusplus::asio::connection& conn) :
    conn(conn)
{
    asyncGetPowerPaths(
        std::bind_front(&PowerStatusMonitor::afterGetPowerPaths, this));
}

bool PowerStatusMonitor::isPowerOn(size_t hostIndex) const
{
    return powerStatusOn.at(hostIndex);
}

void PowerStatusMonitor::handlePowerMatch(sdbusplus::message_t& message)
{
    const std::string path(message.get_path());

    lg2::debug("power match triggered on {PATH}", "PATH", path);

    std::string objectName;
    std::flat_map<std::string, std::variant<std::string>> values;
    message.read(objectName, values);
    auto findState = values.find(HostState::property_names::current_host_state);

    ssize_t hostIndex = managed_host::hostIndexFromPath(path);

    if (hostIndex < 0)
    {
        lg2::error(
            "Could not find host index for {PATH}, cannot update host power status",
            "PATH", path);

        return;
    }

    if (findState != values.end())
    {
        powerStatusOn[hostIndex] =
            std::get<std::string>(findState->second).ends_with("Running");

        lg2::debug("Host {HOSTINDEX} power status: {STATUS}", "HOSTINDEX",
                   hostIndex, "STATUS",
                   (powerStatusOn[hostIndex] ? "on" : "off"));
    }
}

void PowerStatusMonitor::getInitialPowerStatus(size_t hostIndex)
{
    lg2::debug("querying initial power state");

    const std::string powerPath = managed_host::hostPowerPath(hostIndex);

    conn.async_method_call(
        [this, hostIndex,
         powerPath{powerPath}](boost::system::error_code ec,
                               const std::variant<std::string>& state) {
            if (ec)
            {
                return;
            }
            powerStatusOn[hostIndex] =
                std::get<std::string>(state).ends_with("Running");
        },
        HostState::interface, powerPath, em_utils::properties::interface,
        em_utils::properties::get, HostState::interface,
        HostState::property_names::current_host_state);
}

void PowerStatusMonitor::asyncGetPowerPaths(
    const std::function<void(boost::system::error_code ec,
                             const GetSubTreeType& subtree)>& callback)
{
    std::vector<std::string> powerInterfaces = {HostState::interface};
    conn.async_method_call(callback, ObjectMapper::default_service,
                           ObjectMapper::instance_path, ObjectMapper::interface,
                           ObjectMapper::method_names::get_sub_tree, "/", 0,
                           std::move(powerInterfaces));
}

void PowerStatusMonitor::setupPowerMatch(size_t hostIndex)
{
    lg2::debug("setting up power state change match for host {HOSTINDEX}",
               "HOSTINDEX", hostIndex);

    const std::string powerPath = managed_host::hostPowerPath(hostIndex);

    powerMatches.emplace(
        hostIndex,
        sdbusplus::bus::match_t(
            static_cast<sdbusplus::bus_t&>(conn),
            "type='signal',interface='" +
                std::string(em_utils::properties::interface) + "',path='" +
                powerPath + "',arg0='" + std::string(HostState::interface) +
                "'",
            std::bind_front(&PowerStatusMonitor::handlePowerMatch, this)));
}

void PowerStatusMonitor::afterGetPowerPaths(boost::system::error_code ec,
                                            const GetSubTreeType& subtree)
{
    if (ec.value() == ENOENT)
    {
        return; // wasn't found by mapper
    }

    for (const auto& [path, object] : subtree)
    {
        const ssize_t hostIndex = managed_host::hostIndexFromPath(path);

        if (hostIndex < 0)
        {
            continue;
        }

        lg2::debug("found host {HOSTINDEX} power state on path {PATH}",
                   "HOSTINDEX", hostIndex, "PATH", path);

        powerStatusOn[hostIndex] = false;
        setupPowerMatch(hostIndex);
        getInitialPowerStatus(hostIndex);
    }
}

} // namespace power
