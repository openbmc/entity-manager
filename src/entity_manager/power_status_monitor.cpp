#include "power_status_monitor.hpp"

#include "utils.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <sdbusplus/bus/match.hpp>

namespace power
{

const static constexpr char* busname = "xyz.openbmc_project.State.Host";
const static constexpr char* interface = "xyz.openbmc_project.State.Host";
const static constexpr char* path = "/xyz/openbmc_project/state/host0";
const static constexpr char* property = "CurrentHostState";

bool PowerStatusMonitor::isPowerOn()
{
    if (!powerMatch)
    {
        throw std::runtime_error("Power Match Not Created");
    }
    return powerStatusOn;
}

void PowerStatusMonitor::handlePowerMatch(sdbusplus::message_t& message)
{
    std::string objectName;
    boost::container::flat_map<std::string, std::variant<std::string>> values;
    message.read(objectName, values);
    auto findState = values.find(power::property);
    if (findState != values.end())
    {
        powerStatusOn = boost::ends_with(
            std::get<std::string>(findState->second), "Running");
    }
}

void PowerStatusMonitor::setupPowerMatch(
    const std::shared_ptr<sdbusplus::asio::connection>& conn)
{
    powerMatch = std::make_unique<sdbusplus::bus::match_t>(
        static_cast<sdbusplus::bus_t&>(*conn),
        "type='signal',interface='" +
            std::string(em_utils::properties::interface) + "',path='" +
            std::string(power::path) + "',arg0='" +
            std::string(power::interface) + "'",
        std::bind_front(&PowerStatusMonitor::handlePowerMatch, this));

    conn->async_method_call(
        [this](boost::system::error_code ec,
               const std::variant<std::string>& state) {
            if (ec)
            {
                return;
            }
            powerStatusOn =
                boost::ends_with(std::get<std::string>(state), "Running");
        },
        power::busname, power::path, em_utils::properties::interface,
        em_utils::properties::get, power::interface, power::property);
}

} // namespace power
