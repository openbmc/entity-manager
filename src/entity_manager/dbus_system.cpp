#include "dbus_system.hpp"

#include "utils.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/container/flat_set.hpp>

#include <iostream>

class EntityManager;

DbusSystem::DbusSystem(
    EntityManager* em,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus) :
    _em(em), bus(systemBus)
{}

void DbusSystem::getInterfaces(
    const DBusInterfaceInstance& instance,
    std::vector<std::shared_ptr<probe::PerformProbe>>& probes,
    DBusInterface& ret, size_t retries)
{
    if (retries == 0)
    {
        return;
    }

    bus->async_method_call(
        [this, instance, &probes, retries,
         &ret](boost::system::error_code& errc, const DBusInterface& resp) {
            if (errc)
            {
                std::cerr << "error calling getall on  " << instance.busName
                          << " " << instance.path << " "
                          << instance.interface << "\n";

                auto timer = std::make_shared<boost::asio::steady_timer>(io);
                timer->expires_after(std::chrono::seconds(2));

                timer->async_wait([this, timer, instance, &probes, retries,
                                   &ret](const boost::system::error_code&) {
                    getInterfaces(instance, probes, ret, retries - 1);
                });
                return;
            }
            std::cerr << "We die here" << std::endl;
            ret = resp;
            std::cerr << "We died after the assignment" << std::endl;
        },
        instance.busName, instance.path, "org.freedesktop.DBus.Properties",
        "GetAll", instance.interface);
}

void DbusSystem::getObjectMapperSubTree(
    std::vector<std::shared_ptr<probe::PerformProbe>>& probes,
    boost::container::flat_set<std::string>& interfaces, GetSubTreeType ret,
    size_t retries)
{
    // find all connections in the mapper that expose a specific type
    bus->async_method_call(
        [this, interfaces, probes, ret,
         retries](boost::system::error_code& ec,
                  const GetSubTreeType& interfaceSubtree) mutable {
            if (ec)
            {
                if (ec.value() == ENOENT)
                {
                    return; // wasn't found by mapper
                }
                std::cerr << "Error communicating to mapper.\n";

                if (retries == 0U)
                {
                    // if we can't communicate to the mapper something is very
                    // wrong
                    std::exit(EXIT_FAILURE);
                }

                auto timer = std::make_shared<boost::asio::steady_timer>(io);
                timer->expires_after(std::chrono::seconds(10));

                timer->async_wait(
                    [this, timer, interfaces, &probes, retries,
                     ret](const boost::system::error_code&) mutable {
                        getObjectMapperSubTree(probes, interfaces, ret,
                                               retries - 1);
                    });
                return;
            }
            for (const auto& p : interfaceSubtree)
            {
                ret.push_back(p);
            }
        },
        "xyz.openbmc_project.ObjectMapper",
        "/xyz/openbmc_project/object_mapper",
        "xyz.openbmc_project.ObjectMapper", "GetSubTree", "/", maxMapperDepth,
        interfaces);
}
