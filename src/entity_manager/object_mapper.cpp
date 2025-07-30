#include "object_mapper.hpp"

#include "system_mapper.hpp"

#include <boost/container/flat_set.hpp>

#include <iostream>

ObjectMapper::ObjectMapper(
    boost::asio::io_context& io,
    std::shared_ptr<sdbusplus::asio::connection>& systemBus,
    SystemMapper& systemMapper) :
    io(io), systemBus(systemBus), systemMapper(systemMapper)
{}

void ObjectMapper::getInterfaceProperties(
    const DBusInterfaceInstance& instance,
    const std::vector<std::shared_ptr<probe::PerformProbe>>& probeVector,
    MapperGetSubTreeResponse& dbusProbeObjects, size_t retries)
{
    if (retries == 0U)
    {
        std::cerr << "retries exhausted on " << instance.busName << " "
                  << instance.path << " " << instance.interface << "\n";
        return;
    }

    systemBus->async_method_call(
        [this, instance, probeVector, &dbusProbeObjects,
         retries](boost::system::error_code& errc, const DBusInterface& resp) {
            if (errc)
            {
                std::cerr << "error calling getall on  " << instance.busName
                          << " " << instance.path << " "
                          << instance.interface << "\n";

                auto timer = std::make_shared<boost::asio::steady_timer>(io);
                timer->expires_after(std::chrono::seconds(2));

                timer->async_wait(
                    [this, timer, instance, probeVector, &dbusProbeObjects,
                     retries](const boost::system::error_code&) {
                        getInterfaceProperties(instance, probeVector,
                                               dbusProbeObjects, retries - 1);
                    });
                return;
            }

            dbusProbeObjects[instance.path][instance.interface] = resp;
        },
        instance.busName, instance.path, "org.freedesktop.DBus.Properties",
        "GetAll", instance.interface);
}

void ObjectMapper::getSubTree(
    std::vector<std::shared_ptr<probe::PerformProbe>>&& probeVector,
    boost::container::flat_set<std::string>&& interfaces, size_t retries)
{
    // find all connections in the mapper that expose a specific type
    systemBus->async_method_call(
        [this, interfaces, probeVector{std::move(probeVector)},
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
                    [this, timer, interfaces{std::move(interfaces)},
                     probeVector{std::move(probeVector)},
                     retries](const boost::system::error_code&) mutable {
                        getSubTree(std::move(probeVector),
                                   std::move(interfaces), retries - 1);
                    });
                return;
            }

            systemMapper.processDbusObjects(probeVector, interfaceSubtree);
        },
        objectMapperServiceName, objectMapperServicePath,
        objectMapperServiceInterface, objectMapperGetSubTreeCmd, "/",
        maxMapperDepth, interfaces);
}
