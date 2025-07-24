#pragma once

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus/match.hpp>

namespace power
{

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
