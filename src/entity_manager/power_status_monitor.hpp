#pragma once

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus/match.hpp>

namespace power
{

class PowerStatusMonitor
{
  public:
    // @param queryInitialState : whether to query initial power state
    explicit PowerStatusMonitor(sdbusplus::asio::connection& conn,
                                bool queryInitialState);

    bool isPowerOn() const;

  private:
    void handlePowerMatch(sdbusplus::message_t& message);
    void getInitialPowerStatus(sdbusplus::asio::connection& conn);

    bool powerStatusOn = false;
    sdbusplus::bus::match_t powerMatch;
};

} // namespace power
