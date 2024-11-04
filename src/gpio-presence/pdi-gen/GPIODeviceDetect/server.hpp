#pragma once
// NOLINTBEGIN
#include <systemd/sd-bus.h>

#include <sdbusplus/sdbus.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/common.hpp>

#include <limits>
#include <map>
#include <string>

namespace sdbusplus::server::xyz::openbmc_project::configuration
{

class GPIODeviceDetect :
    public sdbusplus::common::xyz::openbmc_project::configuration::
        GPIODeviceDetect
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
    GPIODeviceDetect() = delete;
    GPIODeviceDetect(const GPIODeviceDetect&) = delete;
    GPIODeviceDetect& operator=(const GPIODeviceDetect&) = delete;
    GPIODeviceDetect(GPIODeviceDetect&&) = delete;
    GPIODeviceDetect& operator=(GPIODeviceDetect&&) = delete;
    virtual ~GPIODeviceDetect() = default;

    /** @brief Constructor to put object onto bus at a dbus path.
     *  @param[in] bus - Bus to attach to.
     *  @param[in] path - Path to attach at.
     */
    GPIODeviceDetect(bus_t& bus, const char* path) :
        _xyz_openbmc_project_configuration_GPIODeviceDetect_interface(
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
    GPIODeviceDetect(bus_t& bus, const char* path,
                     const std::map<std::string, PropertiesVariant>& vals,
                     bool skipSignal = false) : GPIODeviceDetect(bus, path)
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
    /** Get value of PresencePinNames */
    virtual std::vector<std::string> presencePinNames() const;
    /** Set value of PresencePinNames with option to skip sending signal */
    virtual std::vector<std::string>
        presencePinNames(std::vector<std::string> value, bool skipSignal);
    /** Set value of PresencePinNames */
    virtual std::vector<std::string>
        presencePinNames(std::vector<std::string> value);
    /** Get value of PresencePinValues */
    virtual std::vector<uint64_t> presencePinValues() const;
    /** Set value of PresencePinValues with option to skip sending signal */
    virtual std::vector<uint64_t>
        presencePinValues(std::vector<uint64_t> value, bool skipSignal);
    /** Set value of PresencePinValues */
    virtual std::vector<uint64_t>
        presencePinValues(std::vector<uint64_t> value);

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
        _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
            .emit_added();
    }

    /** @brief Emit interface removed */
    void emit_removed()
    {
        _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
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

    /** @brief sd-bus callback for get-property 'PresencePinNames' */
    static int _callback_get_PresencePinNames(sd_bus*, const char*, const char*,
                                              const char*, sd_bus_message*,
                                              void*, sd_bus_error*);
    /** @brief sd-bus callback for set-property 'PresencePinNames' */
    static int _callback_set_PresencePinNames(sd_bus*, const char*, const char*,
                                              const char*, sd_bus_message*,
                                              void*, sd_bus_error*);

    /** @brief sd-bus callback for get-property 'PresencePinValues' */
    static int _callback_get_PresencePinValues(
        sd_bus*, const char*, const char*, const char*, sd_bus_message*, void*,
        sd_bus_error*);
    /** @brief sd-bus callback for set-property 'PresencePinValues' */
    static int _callback_set_PresencePinValues(
        sd_bus*, const char*, const char*, const char*, sd_bus_message*, void*,
        sd_bus_error*);

    static const vtable_t _vtable[];
    sdbusplus::server::interface_t
        _xyz_openbmc_project_configuration_GPIODeviceDetect_interface;
    bus_t& _sdbusplus_bus;
    std::string _name{};

    std::vector<std::string> _presencePinNames{};

    std::vector<uint64_t> _presencePinValues{};
};

} // namespace sdbusplus::server::xyz::openbmc_project::configuration

#ifndef SDBUSPP_REMOVE_DEPRECATED_NAMESPACE
namespace sdbusplus::xyz::openbmc_project::Configuration::server
{

using sdbusplus::server::xyz::openbmc_project::configuration::GPIODeviceDetect;

} // namespace sdbusplus::xyz::openbmc_project::Configuration::server
#endif
// NOLINTEND
