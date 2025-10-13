#include "power_status_monitor.hpp"

#include "utils.hpp"

#include <sdbusplus/bus/match.hpp>
#include <phosphor-logging/lg2.hpp>

#include <flat_map>

namespace power
{

const static constexpr char* busname = "xyz.openbmc_project.State.Host";
const static constexpr char* interface = "xyz.openbmc_project.State.Host";
const static constexpr char* path = "/xyz/openbmc_project/state/host0";
const static constexpr char* property = "CurrentHostState";

PowerStatusMonitor::PowerStatusMonitor(sdbusplus::asio::connection& conn) :

    powerMatch(static_cast<sdbusplus::bus_t&>(conn),
               "type='signal',interface='" +
                   std::string(em_utils::properties::interface) + "',path='" +
                   std::string(power::path) + "',arg0='" +
                   std::string(power::interface) + "'",
               std::bind_front(&PowerStatusMonitor::handlePowerMatch, this))

{
    getInitialPowerStatus(conn);
}

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
    conn.async_method_call(
        [this](boost::system::error_code ec,
               const std::variant<std::string>& state) {
            if (ec)
            {
                return;
            }
            powerStatusOn = std::get<std::string>(state).ends_with("Running");
        },
        power::busname, power::path, em_utils::properties::interface,
        em_utils::properties::get, power::interface, power::property);
}

} // namespace power
