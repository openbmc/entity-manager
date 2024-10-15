#include "machine_context.hpp"

#include <thread>

int main()
{
    static constexpr auto reqDBusPath = "/xyz/openbmc_project/MachineContext";
    static constexpr auto reqDBusName = "xyz.openbmc_project.MachineContext";

    sdbusplus::async::context ctx;
    sdbusplus::server::manager_t manager{ctx, reqDBusPath};
    MachineContext mc{ctx, reqDBusPath};

    std::thread nameReqThread([&ctx] { ctx.request_name(reqDBusName); });
    nameReqThread.join();

    ctx.run();

    return 0;
};
