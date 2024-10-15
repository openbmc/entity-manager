#include "machine_context.hpp"

#include <memory>

int main()
{
    static constexpr auto reqDBusPath = "/xyz/openbmc_project/MachineContext";
    static constexpr auto reqDBusName = "xyz.openbmc_project.MachineContext";

    /*Note: OpenBMC convention typically has service name = bus name,
    where the bus name is representative of the underlying hardware.

    In the case of MachineContext, the BMC is not gathering data from
    specific hardware, but is instead parsing device-tree nodes for
    context about the hardware OpenBMC is running on.

    Because the VPD data being parsed is coming from device-tree,
    the daemon and matching service name reflect that.

    Because the parsed data represents 'machine context' data,
    the bus name and associated path the daemon writes to
    reflects that instead.
    */

    sdbusplus::async::context ctx;
    sdbusplus::server::manager_t manager{ctx, reqDBusPath};

    std::unique_ptr<MachineContext> mc = nullptr;
    if (MachineContext::keyNodeExists())
    {
        mc = std::make_unique<MachineContext>(ctx, reqDBusPath);
        mc->populateFromDeviceTree();
    }

    // NOLINTNEXTLINE(readability-static-accessed-through-instance)
    ctx.spawn([](sdbusplus::async::context& ctx) -> sdbusplus::async::task<> {
        ctx.request_name(reqDBusName);
        co_return;
    }(ctx));

    ctx.run();

    return 0;
};
