#pragma once
// NOLINTBEGIN
#include "common.hpp"

#include <systemd/sd-bus.h>

#include <sdbusplus/sdbus.hpp>
#include <sdbusplus/server.hpp>

#include <limits>
#include <map>
#include <string>

namespace sdbusplus::server::xyz::openbmc_project::inventory::source
{

class DevicePresence :
    public sdbusplus::common::xyz::openbmc_project::inventory::source::
        DevicePresence
{
  public:
    /* Define all of the basic class operations:
     *     Not allowed:
     *         - Default constructor to avoid nullptrs.
     *         - Copy operations due to internal unique_ptr.
     *         - Move operations due to 'this' being registered as the
     *           'context' with sdbus.
     *     Allowed:
     *         - Destructor.
     */
    DevicePresence() = delete;
    DevicePresence(const DevicePresence&) = delete;
    DevicePresence& operator=(const DevicePresence&) = delete;
    DevicePresence(DevicePresence&&) = delete;
    DevicePresence& operator=(DevicePresence&&) = delete;
    virtual ~DevicePresence() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     */
    DevicePresence(bus_t& bus, const char* path) :
        _xyz_openbmc_project_inventory_source_DevicePresence_interface(
            bus, path, interface, _vtable, this),
        _sdbusplus_bus(bus)
    {}

    /** @brief Constructor to initialize the object from a map of
     *         properties.
     *
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     *  @param[in] vals - Map of property name to value for initialization.
     */
    DevicePresence(bus_t& bus, const char* path,
                   const std::map<std::string, PropertiesVariant>& vals,
                   bool skipSignal = false) : DevicePresence(bus, path)
    {
        for (const auto& v : vals)
        {
            setPropertyByName(v.first, v.second, skipSignal);
        }
    }

    /** Get value of Name */
    virtual std::string name() const;
    /** Set value of Name with option to skip sending signal */
    virtual std::string name(std::string value, bool skipSignal);
    /** Set value of Name */
    virtual std::string name(std::string value);

    /** @brief Sets a property by name.
     *  @param[in] _name - A string representation of the property name.
     *  @param[in] val - A variant containing the value to set.
     */
    void setPropertyByName(const std::string& _name,
                           const PropertiesVariant& val,
                           bool skipSignal = false);

    /** @brief Gets a property by name.
     *  @param[in] _name - A string representation of the property name.
     *  @return - A variant containing the value of the property.
     */
    PropertiesVariant getPropertyByName(const std::string& _name);

    /** @brief Emit interface added */
    void emit_added()
    {
        _xyz_openbmc_project_inventory_source_DevicePresence_interface
            .emit_added();
    }

    /** @brief Emit interface removed */
    void emit_removed()
    {
        _xyz_openbmc_project_inventory_source_DevicePresence_interface
            .emit_removed();
    }

    /** @return the bus instance */
    bus_t& get_bus()
    {
        return _sdbusplus_bus;
    }

  private:
    /** @brief sd-bus callback for get-property 'Name' */
    static int _callback_get_Name(sd_bus*, const char*, const char*,
                                  const char*, sd_bus_message*, void*,
                                  sd_bus_error*);
    /** @brief sd-bus callback for set-property 'Name' */
    static int _callback_set_Name(sd_bus*, const char*, const char*,
                                  const char*, sd_bus_message*, void*,
                                  sd_bus_error*);

    static const vtable_t _vtable[];
    sdbusplus::server::interface_t
        _xyz_openbmc_project_inventory_source_DevicePresence_interface;
    bus_t& _sdbusplus_bus;
    std::string _name{};
};

} // namespace sdbusplus::server::xyz::openbmc_project::inventory::source

#ifndef SDBUSPP_REMOVE_DEPRECATED_NAMESPACE
namespace sdbusplus::xyz::openbmc_project::Inventory::Source::server
{

using sdbusplus::server::xyz::openbmc_project::inventory::source::
    DevicePresence;

} // namespace sdbusplus::xyz::openbmc_project::Inventory::Source::server
#endif
// NOLINTEND
