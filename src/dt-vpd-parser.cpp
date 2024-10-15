#include "machinecontext.hpp"

int main()
{
    sdbusplus::async::context ctx;

    sdbusplus::server::manager_t manager{ctx, MachineContext::reqDBusPath};
    MachineContext mc{ctx, MachineContext::reqDBusPath};

    ctx.spawn([](sdbusplus::async::context& ctx) -> sdbusplus::async::task<> {
        ctx.request_name(MachineContext::reqDBusInterface);
        co_return;
    }(ctx));

    ctx.run();

    return 0;
};
