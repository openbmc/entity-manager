#pragma once
// NOLINTBEGIN
#include <sdbusplus/async/server.hpp>
#include <sdbusplus/server/interface.hpp>
#include <sdbusplus/server/transaction.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/common.hpp>

#include <type_traits>

namespace sdbusplus::aserver::xyz::openbmc_project::configuration
{

namespace details
{
// forward declaration
template <typename Instance, typename Server>
class GPIODeviceDetect;
} // namespace details

template <typename Instance, typename Server = void>
struct GPIODeviceDetect :
    public std::conditional_t<
        std::is_void_v<Server>,
        sdbusplus::async::server_t<Instance, details::GPIODeviceDetect>,
        details::GPIODeviceDetect<Instance, Server>>
{
    template <typename... Args>
    GPIODeviceDetect(Args&&... args) :
        std::conditional_t<
            std::is_void_v<Server>,
            sdbusplus::async::server_t<Instance, details::GPIODeviceDetect>,
            details::GPIODeviceDetect<Instance, Server>>(
            std::forward<Args>(args)...)
    {}
};

namespace details
{

namespace server_details = sdbusplus::async::server::details;

template <typename Instance, typename Server>
class GPIODeviceDetect :
    public sdbusplus::common::xyz::openbmc_project::configuration::
        GPIODeviceDetect,
    protected server_details::server_context_friend
{
  public:
    explicit GPIODeviceDetect(const char* path) :
        _xyz_openbmc_project_configuration_GPIODeviceDetect_interface(
            _context(), path, interface, _vtable, this)
    {}

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

    /* Property access tags. */
    struct name_t
    {
        using value_type = std::string;
        name_t() = default;
        explicit name_t(value_type) {}
    };
    struct presence_pin_names_t
    {
        using value_type = std::vector<std::string>;
        presence_pin_names_t() = default;
        explicit presence_pin_names_t(value_type) {}
    };
    struct presence_pin_values_t
    {
        using value_type = std::vector<uint64_t>;
        presence_pin_values_t() = default;
        explicit presence_pin_values_t(value_type) {}
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

    auto presence_pin_names() const
        requires server_details::has_get_property_nomsg<presence_pin_names_t,
                                                        Instance>
    {
        return static_cast<const Instance*>(this)->get_property(
            presence_pin_names_t{});
    }
    auto presence_pin_names(sdbusplus::message_t& m) const
        requires server_details::has_get_property_msg<presence_pin_names_t,
                                                      Instance>
    {
        return static_cast<const Instance*>(this)->get_property(
            presence_pin_names_t{}, m);
    }
    auto presence_pin_names() const noexcept
        requires(
            !server_details::has_get_property<presence_pin_names_t, Instance>)
    {
        static_assert(!server_details::has_get_property_missing_const<
                          presence_pin_names_t, Instance>,
                      "Missing const on get_property(presence_pin_names_t)?");
        return presence_pin_names_;
    }

    auto presence_pin_values() const
        requires server_details::has_get_property_nomsg<presence_pin_values_t,
                                                        Instance>
    {
        return static_cast<const Instance*>(this)->get_property(
            presence_pin_values_t{});
    }
    auto presence_pin_values(sdbusplus::message_t& m) const
        requires server_details::has_get_property_msg<presence_pin_values_t,
                                                      Instance>
    {
        return static_cast<const Instance*>(this)->get_property(
            presence_pin_values_t{}, m);
    }
    auto presence_pin_values() const noexcept
        requires(
            !server_details::has_get_property<presence_pin_values_t, Instance>)
    {
        static_assert(!server_details::has_get_property_missing_const<
                          presence_pin_values_t, Instance>,
                      "Missing const on get_property(presence_pin_values_t)?");
        return presence_pin_values_;
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
            _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
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
            _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
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
            _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
                .property_changed("Name");
        }
    }

    template <bool EmitSignal = true, typename Arg = std::vector<std::string>>
    void presence_pin_names(Arg&& new_value)
        requires server_details::has_set_property_nomsg<
            presence_pin_names_t, Instance, std::vector<std::string>>
    {
        bool changed = static_cast<Instance*>(this)->set_property(
            presence_pin_names_t{}, std::forward<Arg>(new_value));

        if (changed && EmitSignal)
        {
            _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
                .property_changed("PresencePinNames");
        }
    }

    template <bool EmitSignal = true, typename Arg = std::vector<std::string>>
    void presence_pin_names(sdbusplus::message_t& m, Arg&& new_value)
        requires server_details::has_set_property_msg<
            presence_pin_names_t, Instance, std::vector<std::string>>
    {
        bool changed = static_cast<Instance*>(this)->set_property(
            presence_pin_names_t{}, m, std::forward<Arg>(new_value));

        if (changed && EmitSignal)
        {
            _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
                .property_changed("PresencePinNames");
        }
    }

    template <bool EmitSignal = true, typename Arg = std::vector<std::string>>
    void presence_pin_names(Arg&& new_value)
        requires(!server_details::has_set_property<
                 presence_pin_names_t, Instance, std::vector<std::string>>)
    {
        static_assert(
            !server_details::has_get_property<presence_pin_names_t, Instance>,
            "Cannot create default set-property for 'presence_pin_names_t' with get-property overload.");

        bool changed = (new_value != presence_pin_names_);
        presence_pin_names_ = std::forward<Arg>(new_value);

        if (changed && EmitSignal)
        {
            _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
                .property_changed("PresencePinNames");
        }
    }

    template <bool EmitSignal = true, typename Arg = std::vector<uint64_t>>
    void presence_pin_values(Arg&& new_value)
        requires server_details::has_set_property_nomsg<
            presence_pin_values_t, Instance, std::vector<uint64_t>>
    {
        bool changed = static_cast<Instance*>(this)->set_property(
            presence_pin_values_t{}, std::forward<Arg>(new_value));

        if (changed && EmitSignal)
        {
            _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
                .property_changed("PresencePinValues");
        }
    }

    template <bool EmitSignal = true, typename Arg = std::vector<uint64_t>>
    void presence_pin_values(sdbusplus::message_t& m, Arg&& new_value)
        requires server_details::has_set_property_msg<
            presence_pin_values_t, Instance, std::vector<uint64_t>>
    {
        bool changed = static_cast<Instance*>(this)->set_property(
            presence_pin_values_t{}, m, std::forward<Arg>(new_value));

        if (changed && EmitSignal)
        {
            _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
                .property_changed("PresencePinValues");
        }
    }

    template <bool EmitSignal = true, typename Arg = std::vector<uint64_t>>
    void presence_pin_values(Arg&& new_value)
        requires(!server_details::has_set_property<
                 presence_pin_values_t, Instance, std::vector<uint64_t>>)
    {
        static_assert(
            !server_details::has_get_property<presence_pin_values_t, Instance>,
            "Cannot create default set-property for 'presence_pin_values_t' with get-property overload.");

        bool changed = (new_value != presence_pin_values_);
        presence_pin_values_ = std::forward<Arg>(new_value);

        if (changed && EmitSignal)
        {
            _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
                .property_changed("PresencePinValues");
        }
    }

  protected:
    std::string name_{};
    std::vector<std::string> presence_pin_names_{};
    std::vector<uint64_t> presence_pin_values_{};

  private:
    /** @return the async context */
    sdbusplus::async::context& _context()
    {
        return server_details::server_context_friend::context<
            Server, GPIODeviceDetect>(this);
    }

    sdbusplus::server::interface_t
        _xyz_openbmc_project_configuration_GPIODeviceDetect_interface;

    static constexpr auto _property_typeid_name =
        utility::tuple_to_array(message::types::type_id<std::string>());
    static constexpr auto _property_typeid_presence_pin_names =
        utility::tuple_to_array(
            message::types::type_id<std::vector<std::string>>());
    static constexpr auto _property_typeid_presence_pin_values =
        utility::tuple_to_array(
            message::types::type_id<std::vector<uint64_t>>());

    static int _callback_get_name(
        sd_bus*, const char*, const char*, const char*, sd_bus_message* reply,
        void* context, sd_bus_error* error [[maybe_unused]])
    {
        auto self = static_cast<GPIODeviceDetect*>(context);

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
        auto self = static_cast<GPIODeviceDetect*>(context);

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

    static int _callback_get_presence_pin_names(
        sd_bus*, const char*, const char*, const char*, sd_bus_message* reply,
        void* context, sd_bus_error* error [[maybe_unused]])
    {
        auto self = static_cast<GPIODeviceDetect*>(context);

        try
        {
            auto m = sdbusplus::message_t{reply};

            // Set up the transaction.
            server::transaction::set_id(m);

            // Get property value and add to message.
            if constexpr (server_details::has_get_property_msg<
                              presence_pin_names_t, Instance>)
            {
                auto v = self->presence_pin_names(m);
                static_assert(
                    std::is_convertible_v<decltype(v),
                                          std::vector<std::string>>,
                    "Property doesn't convert to 'std::vector<std::string>'.");
                m.append<std::vector<std::string>>(std::move(v));
            }
            else
            {
                auto v = self->presence_pin_names();
                static_assert(
                    std::is_convertible_v<decltype(v),
                                          std::vector<std::string>>,
                    "Property doesn't convert to 'std::vector<std::string>'.");
                m.append<std::vector<std::string>>(std::move(v));
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

    static int _callback_set_presence_pin_names(
        sd_bus*, const char*, const char*, const char*,
        sd_bus_message* value [[maybe_unused]], void* context [[maybe_unused]],
        sd_bus_error* error [[maybe_unused]])
    {
        auto self = static_cast<GPIODeviceDetect*>(context);

        try
        {
            auto m = sdbusplus::message_t{value};

            // Set up the transaction.
            server::transaction::set_id(m);

            auto new_value = m.unpack<std::vector<std::string>>();

            // Get property value and add to message.
            if constexpr (server_details::has_set_property_msg<
                              presence_pin_names_t, Instance,
                              std::vector<std::string>>)
            {
                self->presence_pin_names(m, std::move(new_value));
            }
            else
            {
                self->presence_pin_names(std::move(new_value));
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

    static int _callback_get_presence_pin_values(
        sd_bus*, const char*, const char*, const char*, sd_bus_message* reply,
        void* context, sd_bus_error* error [[maybe_unused]])
    {
        auto self = static_cast<GPIODeviceDetect*>(context);

        try
        {
            auto m = sdbusplus::message_t{reply};

            // Set up the transaction.
            server::transaction::set_id(m);

            // Get property value and add to message.
            if constexpr (server_details::has_get_property_msg<
                              presence_pin_values_t, Instance>)
            {
                auto v = self->presence_pin_values(m);
                static_assert(
                    std::is_convertible_v<decltype(v), std::vector<uint64_t>>,
                    "Property doesn't convert to 'std::vector<uint64_t>'.");
                m.append<std::vector<uint64_t>>(std::move(v));
            }
            else
            {
                auto v = self->presence_pin_values();
                static_assert(
                    std::is_convertible_v<decltype(v), std::vector<uint64_t>>,
                    "Property doesn't convert to 'std::vector<uint64_t>'.");
                m.append<std::vector<uint64_t>>(std::move(v));
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

    static int _callback_set_presence_pin_values(
        sd_bus*, const char*, const char*, const char*,
        sd_bus_message* value [[maybe_unused]], void* context [[maybe_unused]],
        sd_bus_error* error [[maybe_unused]])
    {
        auto self = static_cast<GPIODeviceDetect*>(context);

        try
        {
            auto m = sdbusplus::message_t{value};

            // Set up the transaction.
            server::transaction::set_id(m);

            auto new_value = m.unpack<std::vector<uint64_t>>();

            // Get property value and add to message.
            if constexpr (server_details::has_set_property_msg<
                              presence_pin_values_t, Instance,
                              std::vector<uint64_t>>)
            {
                self->presence_pin_values(m, std::move(new_value));
            }
            else
            {
                self->presence_pin_values(std::move(new_value));
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
        vtable::property(
            "PresencePinNames", _property_typeid_presence_pin_names.data(),
            _callback_get_presence_pin_names, _callback_set_presence_pin_names,
            vtable::property_::emits_change),
        vtable::property(
            "PresencePinValues", _property_typeid_presence_pin_values.data(),
            _callback_get_presence_pin_values,
            _callback_set_presence_pin_values, vtable::property_::emits_change),

        vtable::end(),
    };
};

} // namespace details
} // namespace sdbusplus::aserver::xyz::openbmc_project::configuration
// NOLINTEND
