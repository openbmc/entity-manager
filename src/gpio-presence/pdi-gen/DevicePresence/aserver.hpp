#pragma once

// NOLINTBEGIN

#include "common.hpp"

#include <sdbusplus/async/server.hpp>
#include <sdbusplus/server/interface.hpp>
#include <sdbusplus/server/transaction.hpp>

#include <type_traits>

namespace sdbusplus::aserver::xyz::openbmc_project::inventory::source
{

namespace details
{
// forward declaration
template <typename Instance, typename Server>
class DevicePresence;
} // namespace details

template <typename Instance, typename Server = void>
struct DevicePresence :
    public std::conditional_t<
        std::is_void_v<Server>,
        sdbusplus::async::server_t<Instance, details::DevicePresence>,
        details::DevicePresence<Instance, Server>>
{
    template <typename... Args>
    DevicePresence(Args&&... args) :
        std::conditional_t<
            std::is_void_v<Server>,
            sdbusplus::async::server_t<Instance, details::DevicePresence>,
            details::DevicePresence<Instance, Server>>(
            std::forward<Args>(args)...)
    {}
};

namespace details
{

namespace server_details = sdbusplus::async::server::details;

template <typename Instance, typename Server>
class DevicePresence :
    public sdbusplus::common::xyz::openbmc_project::inventory::source::
        DevicePresence,
    protected server_details::server_context_friend
{
  public:
    explicit DevicePresence(const char* path) :
        _xyz_openbmc_project_inventory_source_DevicePresence_interface(
            _context(), path, interface, _vtable, this)
    {}

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

    /* Property access tags. */
    struct name_t
    {
        using value_type = std::string;
        name_t() = default;
        explicit name_t(value_type) {}
    };

    /* Method tags. */

    auto name() const
        requires server_details::has_get_property_nomsg<name_t, Instance>
    {
        return static_cast<const Instance*>(this)->get_property(name_t{});
    }
    auto name(sdbusplus::message_t& m) const
        requires server_details::has_get_property_msg<name_t, Instance>
    {
        return static_cast<const Instance*>(this)->get_property(name_t{}, m);
    }
    auto name() const noexcept
        requires(!server_details::has_get_property<name_t, Instance>)
    {
        static_assert(
            !server_details::has_get_property_missing_const<name_t, Instance>,
            "Missing const on get_property(name_t)?");
        return name_;
    }

    template <bool EmitSignal = true, typename Arg = std::string>
    void name(Arg&& new_value)
        requires server_details::has_set_property_nomsg<name_t, Instance,
                                                        std::string>
    {
        bool changed = static_cast<Instance*>(this)->set_property(
            name_t{}, std::forward<Arg>(new_value));

        if (changed && EmitSignal)
        {
            _xyz_openbmc_project_inventory_source_DevicePresence_interface
                .property_changed("Name");
        }
    }

    template <bool EmitSignal = true, typename Arg = std::string>
    void name(sdbusplus::message_t& m, Arg&& new_value)
        requires server_details::has_set_property_msg<name_t, Instance,
                                                      std::string>
    {
        bool changed = static_cast<Instance*>(this)->set_property(
            name_t{}, m, std::forward<Arg>(new_value));

        if (changed && EmitSignal)
        {
            _xyz_openbmc_project_inventory_source_DevicePresence_interface
                .property_changed("Name");
        }
    }

    template <bool EmitSignal = true, typename Arg = std::string>
    void name(Arg&& new_value)
        requires(
            !server_details::has_set_property<name_t, Instance, std::string>)
    {
        static_assert(
            !server_details::has_get_property<name_t, Instance>,
            "Cannot create default set-property for 'name_t' with get-property overload.");

        bool changed = (new_value != name_);
        name_ = std::forward<Arg>(new_value);

        if (changed && EmitSignal)
        {
            _xyz_openbmc_project_inventory_source_DevicePresence_interface
                .property_changed("Name");
        }
    }

  protected:
    std::string name_{};

  private:
    /** @return the async context */
    sdbusplus::async::context& _context()
    {
        return server_details::server_context_friend::context<
            Server, DevicePresence>(this);
    }

    sdbusplus::server::interface_t
        _xyz_openbmc_project_inventory_source_DevicePresence_interface;

    static constexpr auto _property_typeid_name =
        utility::tuple_to_array(message::types::type_id<std::string>());

    static int _callback_get_name(
        sd_bus*, const char*, const char*, const char*, sd_bus_message* reply,
        void* context, sd_bus_error* error [[maybe_unused]])
    {
        auto self = static_cast<DevicePresence*>(context);

        try
        {
            auto m = sdbusplus::message_t{reply};

            // Set up the transaction.
            server::transaction::set_id(m);

            // Get property value and add to message.
            if constexpr (server_details::has_get_property_msg<name_t,
                                                               Instance>)
            {
                auto v = self->name(m);
                static_assert(std::is_convertible_v<decltype(v), std::string>,
                              "Property doesn't convert to 'std::string'.");
                m.append<std::string>(std::move(v));
            }
            else
            {
                auto v = self->name();
                static_assert(std::is_convertible_v<decltype(v), std::string>,
                              "Property doesn't convert to 'std::string'.");
                m.append<std::string>(std::move(v));
            }
        }
        catch (const std::exception&)
        {
            self->_context().get_bus().set_current_exception(
                std::current_exception());
            return -EINVAL;
        }

        return 1;
    }

    static int _callback_set_name(
        sd_bus*, const char*, const char*, const char*,
        sd_bus_message* value [[maybe_unused]], void* context [[maybe_unused]],
        sd_bus_error* error [[maybe_unused]])
    {
        auto self = static_cast<DevicePresence*>(context);

        try
        {
            auto m = sdbusplus::message_t{value};

            // Set up the transaction.
            server::transaction::set_id(m);

            auto new_value = m.unpack<std::string>();

            // Get property value and add to message.
            if constexpr (server_details::has_set_property_msg<name_t, Instance,
                                                               std::string>)
            {
                self->name(m, std::move(new_value));
            }
            else
            {
                self->name(std::move(new_value));
            }
        }
        catch (const std::exception&)
        {
            self->_context().get_bus().set_current_exception(
                std::current_exception());
            return -EINVAL;
        }

        return 1;
    }

    static constexpr sdbusplus::vtable_t _vtable[] = {
        vtable::start(),

        vtable::property("Name", _property_typeid_name.data(),
                         _callback_get_name, _callback_set_name,
                         vtable::property_::emits_change),

        vtable::end(),
    };
};

} // namespace details
} // namespace sdbusplus::aserver::xyz::openbmc_project::inventory::source

// NOLINTEND
