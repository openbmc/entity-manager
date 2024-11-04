#pragma once
// NOLINTBEGIN
#include "common.hpp"

#include <sdbusplus/async/client.hpp>

#include <type_traits>

namespace sdbusplus::client::xyz::openbmc_project::inventory::source
{

namespace details
{
// forward declaration
template <typename Client, typename Proxy>
class DevicePresence;
} // namespace details

/** Alias class so we can use the client in both a client_t aggregation
 *  and individually.
 *
 *  sdbusplus::async::client_t<DevicePresence>() or
 *  DevicePresence() both construct an equivalent instance.
 */
template <typename Client = void, typename Proxy = void>
struct DevicePresence :
    public std::conditional_t<
        std::is_void_v<Client>,
        sdbusplus::async::client_t<details::DevicePresence>,
        details::DevicePresence<Client, Proxy>>
{
    template <typename... Args>
    DevicePresence(Args&&... args) :
        std::conditional_t<std::is_void_v<Client>,
                           sdbusplus::async::client_t<details::DevicePresence>,
                           details::DevicePresence<Client, Proxy>>(
            std::forward<Args>(args)...)
    {}
};

namespace details
{

template <typename Client, typename Proxy>
class DevicePresence :
    public sdbusplus::common::xyz::openbmc_project::inventory::source::
        DevicePresence,
    private sdbusplus::async::client::details::client_context_friend
{
  public:
    friend Client;
    template <typename, typename>
    friend struct sdbusplus::client::xyz::openbmc_project::inventory::source::
        DevicePresence;

    // Delete default constructor as these should only be constructed
    // indirectly through sdbusplus::async::client_t.
    DevicePresence() = delete;

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

  private:
    // Conversion constructor from proxy used by client_t.
    explicit constexpr DevicePresence(Proxy p) : proxy(p.interface(interface))
    {}

    sdbusplus::async::context& context()
    {
        return sdbusplus::async::client::details::client_context_friend::
            context<Client, DevicePresence>(this);
    }

    decltype(std::declval<Proxy>().interface(interface)) proxy = {};
};

} // namespace details

} // namespace sdbusplus::client::xyz::openbmc_project::inventory::source
// NOLINTEND
