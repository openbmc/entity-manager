#include "power_status_monitor.hpp"

#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus/match.hpp>
#include <xyz/openbmc_project/State/Host/client.hpp>

#include <flat_map>

namespace power
{

using HostState = sdbusplus::common::xyz::openbmc_project::state::Host;

const static std::string path =
    std::format("{}/{}{}", HostState::namespace_path::value,
                HostState::namespace_path::host, 0);
const static constexpr char* property = "CurrentHostState";

PowerStatusMonitor::PowerStatusMonitor(sdbusplus::asio::connection& conn) :

    powerMatch(static_cast<sdbusplus::bus_t&>(conn),
               "type='signal',interface='" +
                   std::string(em_utils::properties::interface) + "',path='" +
                   std::string(power::path) + "',arg0='" +
                   std::string(HostState::interface) + "'",
               std::bind_front(&PowerStatusMonitor::handlePowerMatch, this))

{}

bool PowerStatusMonitor::isPowerOn() const
{
    return powerStatusOn;
}

void PowerStatusMonitor::handlePowerMatch(sdbusplus::message_t& message)
{
    lg2::debug("power match triggered");

    std::string objectName;
    std::flat_map<std::string, std::variant<std::string>> values;
    message.read(objectName, values);
    auto findState = values.find(power::property);
    if (findState != values.end())
    {
        powerStatusOn =
            std::get<std::string>(findState->second).ends_with("Running");
    }
}

void PowerStatusMonitor::getInitialPowerStatus(
    sdbusplus::asio::connection& conn)
{
    lg2::debug("querying initial power state");

    conn.async_method_call(
        [this](boost::system::error_code ec,
               const std::variant<std::string>& state) {
            if (ec)
            {
                return;
            }
            powerStatusOn = std::get<std::string>(state).ends_with("Running");
        },
        HostState::interface, power::path, em_utils::properties::interface,
        em_utils::properties::get, HostState::interface, power::property);
}

} // namespace power
