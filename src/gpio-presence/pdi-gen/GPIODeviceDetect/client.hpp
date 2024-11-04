#pragma once
// NOLINTBEGIN
#include "common.hpp"

#include <sdbusplus/async/client.hpp>

#include <type_traits>

namespace sdbusplus::client::xyz::openbmc_project::configuration
{

namespace details
{
// forward declaration
template <typename Client, typename Proxy>
class GPIODeviceDetect;
} // namespace details

/** Alias class so we can use the client in both a client_t aggregation
 *  and individually.
 *
 *  sdbusplus::async::client_t<GPIODeviceDetect>() or
 *  GPIODeviceDetect() both construct an equivalent instance.
 */
template <typename Client = void, typename Proxy = void>
struct GPIODeviceDetect :
    public std::conditional_t<
        std::is_void_v<Client>,
        sdbusplus::async::client_t<details::GPIODeviceDetect>,
        details::GPIODeviceDetect<Client, Proxy>>
{
    template <typename... Args>
    GPIODeviceDetect(Args&&... args) :
        std::conditional_t<
            std::is_void_v<Client>,
            sdbusplus::async::client_t<details::GPIODeviceDetect>,
            details::GPIODeviceDetect<Client, Proxy>>(
            std::forward<Args>(args)...)
    {}
};

namespace details
{

template <typename Client, typename Proxy>
class GPIODeviceDetect :
    public sdbusplus::common::xyz::openbmc_project::configuration::
        GPIODeviceDetect,
    private sdbusplus::async::client::details::client_context_friend
{
  public:
    friend Client;
    template <typename, typename>
    friend struct sdbusplus::client::xyz::openbmc_project::configuration::
        GPIODeviceDetect;

    // Delete default constructor as these should only be constructed
    // indirectly through sdbusplus::async::client_t.
    GPIODeviceDetect() = delete;

    /** Get value of Name
     *  Used by entity-manager to identify which hw was detected. For internal
     * use by entity-manager. Fictional examples are
     * 'com.meta.Hardware.Yv4.cable0', 'com.meta.Hardware.Yv4.Fanboard0',
     * 'com.vendorX.Hardware.BoardY.LeakDetector3'. The name should be
     * namespaced with dots to avoid collision, according to documented Hardware
     * enumerations like
     * https://github.com/openbmc/phosphor-dbus-interfaces/blob/6a8507d06e172d8d29c0459f0a0d078553d2ecc7/yaml/com/meta/Hardware/BMC.interface.yaml
     */
    auto name()
    {
        return proxy.template get_property<std::string>(context(), "Name");
    }

    /** Set value of Name
     *  Used by entity-manager to identify which hw was detected. For internal
     * use by entity-manager. Fictional examples are
     * 'com.meta.Hardware.Yv4.cable0', 'com.meta.Hardware.Yv4.Fanboard0',
     * 'com.vendorX.Hardware.BoardY.LeakDetector3'. The name should be
     * namespaced with dots to avoid collision, according to documented Hardware
     * enumerations like
     * https://github.com/openbmc/phosphor-dbus-interfaces/blob/6a8507d06e172d8d29c0459f0a0d078553d2ecc7/yaml/com/meta/Hardware/BMC.interface.yaml
     */
    auto name(auto value)
    {
        return proxy.template set_property<std::string>(
            context(), "Name", std::forward<decltype(value)>(value));
    }

    /** Get value of PresencePinNames
     *  Names of the gpio lines.
     */
    auto presence_pin_names()
    {
        return proxy.template get_property<std::vector<std::string>>(
            context(), "PresencePinNames");
    }

    /** Set value of PresencePinNames
     *  Names of the gpio lines.
     */
    auto presence_pin_names(auto value)
    {
        return proxy.template set_property<std::vector<std::string>>(
            context(), "PresencePinNames",
            std::forward<decltype(value)>(value));
    }

    /** Get value of PresencePinValues
     *  Values of the gpio lines for which a device is considered present.
     * Choosing 'uint64' instead of 'bool' here for compatibility with how EM
     * exposes configuration on dbus.
     */
    auto presence_pin_values()
    {
        return proxy.template get_property<std::vector<uint64_t>>(
            context(), "PresencePinValues");
    }

    /** Set value of PresencePinValues
     *  Values of the gpio lines for which a device is considered present.
     * Choosing 'uint64' instead of 'bool' here for compatibility with how EM
     * exposes configuration on dbus.
     */
    auto presence_pin_values(auto value)
    {
        return proxy.template set_property<std::vector<uint64_t>>(
            context(), "PresencePinValues",
            std::forward<decltype(value)>(value));
    }

  private:
    // Conversion constructor from proxy used by client_t.
    explicit constexpr GPIODeviceDetect(Proxy p) : proxy(p.interface(interface))
    {}

    sdbusplus::async::context& context()
    {
        return sdbusplus::async::client::details::client_context_friend::
            context<Client, GPIODeviceDetect>(this);
    }

    decltype(std::declval<Proxy>().interface(interface)) proxy = {};
};

} // namespace details

} // namespace sdbusplus::client::xyz::openbmc_project::configuration
// NOLINTEND
