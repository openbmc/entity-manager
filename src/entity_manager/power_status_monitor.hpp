#pragma once

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus/match.hpp>

namespace power
{

class PowerStatusMonitor
{
  public:
    explicit PowerStatusMonitor(sdbusplus::asio::connection& conn,
                                bool testing);

    bool isPowerOn() const;

  private:
    void handlePowerMatch(sdbusplus::message_t& message);
    void getInitialPowerStatus(sdbusplus::asio::connection& conn);

    bool powerStatusOn = false;
    sdbusplus::bus::match_t powerMatch;
};

} // namespace power
