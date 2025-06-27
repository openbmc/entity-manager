#pragma once

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus/match.hpp>

namespace power
{

const static constexpr char* busname = "xyz.openbmc_project.State.Host";
const static constexpr char* interface = "xyz.openbmc_project.State.Host";
const static constexpr char* path = "/xyz/openbmc_project/state/host0";
const static constexpr char* property = "CurrentHostState";

class PowerStatusMonitor
{
  public:
    bool isPowerOn();
    void setupPowerMatch(
        const std::shared_ptr<sdbusplus::asio::connection>& conn);

  private:
    bool powerStatusOn = false;
    std::unique_ptr<sdbusplus::bus::match_t> powerMatch = nullptr;
};

} // namespace power
