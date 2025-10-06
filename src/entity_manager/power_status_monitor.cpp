#include "power_status_monitor.hpp"

#include "phosphor-logging/lg2.hpp"
#include "utils.hpp"

#include <sdbusplus/bus/match.hpp>

#include <flat_map>

namespace power
{

const static constexpr char* busname = "xyz.openbmc_project.State.Host";
const static constexpr char* interface = "xyz.openbmc_project.State.Host";
const static constexpr char* property = "CurrentHostState";

PowerStatusMonitor::PowerStatusMonitor(sdbusplus::asio::connection& conn) :
    conn(conn), powerInterfaces({interface})
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
    lg2::debug("power match triggered");

    std::string objectName;
    std::flat_map<std::string, std::variant<std::string>> values;
    message.read(objectName, values);
    auto findState = values.find(power::property);

    std::string path(message.get_path());

    ssize_t hostIndex = -1;

    for (const auto& [k, v] : powerPaths)
    {
        if (v == path)
        {
            hostIndex = k;
        }
    }

    if (hostIndex == -1)
    {
        lg2::error(
            "Could not find host index for {PATH}, cannot update host power status",
            "PATH", path);
    }

    if (findState != values.end())
    {
        powerStatusOn[hostIndex] =
            std::get<std::string>(findState->second).ends_with("Running");

        lg2::debug("Host %d power status: {STATUS}", "STATUS",
                   (powerStatusOn[hostIndex] ? "on" : "off"));
    }
}

void PowerStatusMonitor::getInitialPowerStatus(size_t hostIndex)
{
    conn.async_method_call(
        [this, hostIndex](boost::system::error_code ec,
                          const std::variant<std::string>& state) {
            if (ec)
            {
                return;
            }
            powerStatusOn[hostIndex] =
                std::get<std::string>(state).ends_with("Running");
        },
        power::busname, powerPaths[hostIndex], em_utils::properties::interface,
        em_utils::properties::get, power::interface, power::property);
}

void PowerStatusMonitor::asyncGetPowerPaths(
    const std::function<void(boost::system::error_code ec,
                             const GetSubTreeType& subtree)>& callback)
{
    conn.async_method_call(callback,

                           "xyz.openbmc_project.ObjectMapper",
                           "/xyz/openbmc_project/object_mapper",
                           "xyz.openbmc_project.ObjectMapper", "GetSubTree",
                           "/", 0, powerInterfaces);
}

void PowerStatusMonitor::setupPowerMatch(size_t hostIndex)
{
    powerMatches.emplace(
        hostIndex,
        sdbusplus::bus::match_t(
            static_cast<sdbusplus::bus_t&>(conn),
            "type='signal',interface='" +
                std::string(em_utils::properties::interface) + "',path='" +
                powerPaths[hostIndex] + "',arg0='" +
                std::string(power::interface) + "'",
            std::bind_front(&PowerStatusMonitor::handlePowerMatch, this)));
}

void PowerStatusMonitor::afterGetPowerPaths(boost::system::error_code ec,
                                            const GetSubTreeType& subtree)
{
    std::string basePath = "/xyz/openbmc_project/state/host";
    if (ec.value() == ENOENT)
    {
        return; // wasn't found by mapper
    }

    for (const auto& [path, object] : subtree)
    {
        if (!path.starts_with(basePath))
        {
            continue;
        }
        std::string hostNum = basePath.substr(basePath.size());
        try
        {
            size_t hostIndex = stoul(hostNum);
            powerPaths[hostIndex] = path;

            powerStatusOn[hostIndex] = false;

            setupPowerMatch(hostIndex);

            getInitialPowerStatus(hostIndex);
        }
        catch (std::exception& e)
        {
            lg2::error("could not get host index: {ERR}", "ERR", e);
        }
    }
}

} // namespace power
