#include "machine_context.hpp"

int main()
{
    static constexpr auto reqDBusPath = "/xyz/openbmc_project/MachineContext";
    static constexpr auto reqDBusName = "xyz.openbmc_project.MachineContext";

    sdbusplus::async::context ctx;
    sdbusplus::server::manager_t manager{ctx, reqDBusPath};
    MachineContext mc{ctx, reqDBusPath};

    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    ctx.spawn([](sdbusplus::async::context& ctx) -> sdbusplus::async::task<> {
        ctx.request_name(reqDBusName);
        co_return;
    }(ctx));

    ctx.run();

    return 0;
};
