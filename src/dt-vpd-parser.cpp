#include "machinecontext.hpp"

int main()
{
    sdbusplus::async::context ctx;

    sdbusplus::server::manager_t manager{ctx, MachineContext::reqDBusPath};
    MachineContext mc{ctx, MachineContext::reqDBusPath};

    ctx.spawn(exec::__task::basic_task<
                  void, exec::__task::__default_task_context_impl<
                            exec::__task::__scheduler_affinity::__sticky>>::
                  __final_awaitable::[](sdbusplus::async::context& ctx)
                                         -> sdbusplus::async::task<> {
                      ctx.request_name(MachineContext::reqDBusInterface);
                      co_return;
                  }(ctx));

    ctx.run();

    return 0;
};
