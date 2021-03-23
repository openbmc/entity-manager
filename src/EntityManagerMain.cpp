#include "EntityManager.hpp"

constexpr const char* tempConfigDir = "/tmp/configuration/";
constexpr const char* lastConfiguration = "/tmp/configuration/last.json";

int main()
{
    // setup connection to dbus
    SYSTEM_BUS = std::make_shared<sdbusplus::asio::connection>(io);
    SYSTEM_BUS->request_name("xyz.openbmc_project.EntityManager");

    sdbusplus::asio::object_server objServer(SYSTEM_BUS);

    std::shared_ptr<sdbusplus::asio::dbus_interface> entityIface =
        objServer.add_interface("/xyz/openbmc_project/EntityManager",
                                "xyz.openbmc_project.EntityManager");

    // to keep reference to the match / filter objects so they don't get
    // destroyed

    nlohmann::json systemConfiguration = nlohmann::json::object();

    // We need a poke from DBus for static providers that create all their
    // objects prior to claiming a well-known name, and thus don't emit any
    // org.freedesktop.DBus.Properties signals.  Similarly if a process exits
    // for any reason, expected or otherwise, we'll need a poke to remove
    // entities from DBus.
    sdbusplus::bus::match::match nameOwnerChangedMatch(
        static_cast<sdbusplus::bus::bus&>(*SYSTEM_BUS),
        sdbusplus::bus::match::rules::nameOwnerChanged(),
        [&](sdbusplus::message::message&) {
            propertiesChangedCallback(systemConfiguration, objServer);
        });
    // We also need a poke from DBus when new interfaces are created or
    // destroyed.
    sdbusplus::bus::match::match interfacesAddedMatch(
        static_cast<sdbusplus::bus::bus&>(*SYSTEM_BUS),
        sdbusplus::bus::match::rules::interfacesAdded(),
        [&](sdbusplus::message::message&) {
            propertiesChangedCallback(systemConfiguration, objServer);
        });
    sdbusplus::bus::match::match interfacesRemovedMatch(
        static_cast<sdbusplus::bus::bus&>(*SYSTEM_BUS),
        sdbusplus::bus::match::rules::interfacesRemoved(),
        [&](sdbusplus::message::message&) {
            propertiesChangedCallback(systemConfiguration, objServer);
        });

    io.post(
        [&]() { propertiesChangedCallback(systemConfiguration, objServer); });

    entityIface->register_method("ReScan", [&]() {
        propertiesChangedCallback(systemConfiguration, objServer);
    });
    entityIface->initialize();

    if (fwVersionIsSame())
    {
        if (std::filesystem::is_regular_file(currentConfiguration))
        {
            // this file could just be deleted, but it's nice for debug
            std::filesystem::create_directory(tempConfigDir);
            std::filesystem::remove(lastConfiguration);
            std::filesystem::copy(currentConfiguration, lastConfiguration);
            std::filesystem::remove(currentConfiguration);

            std::ifstream jsonStream(lastConfiguration);
            if (jsonStream.good())
            {
                auto data = nlohmann::json::parse(jsonStream, nullptr, false);
                if (data.is_discarded())
                {
                    std::cerr << "syntax error in " << lastConfiguration
                              << "\n";
                }
                else
                {
                    lastJson = std::move(data);
                }
            }
            else
            {
                std::cerr << "unable to open " << lastConfiguration << "\n";
            }
        }
    }
    else
    {
        // not an error, just logging at this level to make it in the journal
        std::cerr << "Clearing previous configuration\n";
        std::filesystem::remove(currentConfiguration);
    }

    // some boards only show up after power is on, we want to not say they are
    // removed until the same state happens
    setupPowerMatch(SYSTEM_BUS);

    io.run();

    return 0;
}
