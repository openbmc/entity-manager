#pragma once

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/bus/match.hpp>

namespace power
{

using GetSubTreeType = std::vector<
    std::pair<std::string,
              std::vector<std::pair<std::string, std::vector<std::string>>>>>;

class PowerStatusMonitor
{
  public:
    explicit PowerStatusMonitor(sdbusplus::asio::connection& conn);

    bool isPowerOn(size_t hostIndex) const;

  private:
    void handlePowerMatch(sdbusplus::message_t& message);
    void getInitialPowerStatus(size_t hostIndex);
    void setupPowerMatch(size_t hostIndex);

    void asyncGetPowerPaths(
        const std::function<void(boost::system::error_code ec,
                                 const GetSubTreeType& subtree)>& callback);
    void afterGetPowerPaths(boost::system::error_code ec,
                            const GetSubTreeType& subtree);

    sdbusplus::asio::connection& conn;

    std::map<size_t, bool> powerStatusOn;
    std::map<size_t, sdbusplus::bus::match_t> powerMatches;

    // map of host index -> object path,
    // to be able to form references for async callbacks
    std::map<size_t, std::string> powerPaths;

    std::vector<std::string> powerInterfaces;
};

} // namespace power
