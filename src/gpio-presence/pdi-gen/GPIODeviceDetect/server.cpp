// NOLINTBEGIN
#include <sdbusplus/sdbus.hpp>
#include <sdbusplus/sdbuspp_support/server.hpp>
#include <sdbusplus/server.hpp>
#include <xyz/openbmc_project/Configuration/GPIODeviceDetect/server.hpp>

#include <exception>
#include <map>
#include <string>
#include <tuple>

namespace sdbusplus::server::xyz::openbmc_project::configuration
{

auto GPIODeviceDetect::name() const -> std::string
{
    return _name;
}

int GPIODeviceDetect::_callback_get_Name(
    sd_bus* /*bus*/, const char* /*path*/, const char* /*interface*/,
    const char* /*property*/, sd_bus_message* reply, void* context,
    sd_bus_error* error)
{
    auto o = static_cast<GPIODeviceDetect*>(context);

    try
    {
        return sdbusplus::sdbuspp::property_callback(
            reply, o->get_bus().getInterface(), error,
            std::function([=]() { return o->name(); }));
    }
    catch (const std::exception&)
    {
        o->get_bus().set_current_exception(std::current_exception());
        return 1;
    }
}

auto GPIODeviceDetect::name(std::string value, bool skipSignal) -> std::string
{
    if (_name != value)
    {
        _name = value;
        if (!skipSignal)
        {
            _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
                .property_changed("Name");
        }
    }

    return _name;
}

auto GPIODeviceDetect::name(std::string val) -> std::string
{
    return name(val, false);
}

int GPIODeviceDetect::_callback_set_Name(
    sd_bus* /*bus*/, const char* /*path*/, const char* /*interface*/,
    const char* /*property*/, sd_bus_message* value, void* context,
    sd_bus_error* error)
{
    auto o = static_cast<GPIODeviceDetect*>(context);

    try
    {
        return sdbusplus::sdbuspp::property_callback(
            value, o->get_bus().getInterface(), error,
            std::function([=](std::string&& arg) { o->name(std::move(arg)); }));
    }
    catch (const std::exception&)
    {
        o->get_bus().set_current_exception(std::current_exception());
        return 1;
    }
}

namespace details
{
namespace GPIODeviceDetect
{
static const auto _property_Name =
    utility::tuple_to_array(message::types::type_id<std::string>());
}
} // namespace details

auto GPIODeviceDetect::presencePinNames() const -> std::vector<std::string>
{
    return _presencePinNames;
}

int GPIODeviceDetect::_callback_get_PresencePinNames(
    sd_bus* /*bus*/, const char* /*path*/, const char* /*interface*/,
    const char* /*property*/, sd_bus_message* reply, void* context,
    sd_bus_error* error)
{
    auto o = static_cast<GPIODeviceDetect*>(context);

    try
    {
        return sdbusplus::sdbuspp::property_callback(
            reply, o->get_bus().getInterface(), error,
            std::function([=]() { return o->presencePinNames(); }));
    }
    catch (const std::exception&)
    {
        o->get_bus().set_current_exception(std::current_exception());
        return 1;
    }
}

auto GPIODeviceDetect::presencePinNames(std::vector<std::string> value,
                                        bool skipSignal)
    -> std::vector<std::string>
{
    if (_presencePinNames != value)
    {
        _presencePinNames = value;
        if (!skipSignal)
        {
            _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
                .property_changed("PresencePinNames");
        }
    }

    return _presencePinNames;
}

auto GPIODeviceDetect::presencePinNames(std::vector<std::string> val)
    -> std::vector<std::string>
{
    return presencePinNames(val, false);
}

int GPIODeviceDetect::_callback_set_PresencePinNames(
    sd_bus* /*bus*/, const char* /*path*/, const char* /*interface*/,
    const char* /*property*/, sd_bus_message* value, void* context,
    sd_bus_error* error)
{
    auto o = static_cast<GPIODeviceDetect*>(context);

    try
    {
        return sdbusplus::sdbuspp::property_callback(
            value, o->get_bus().getInterface(), error,
            std::function([=](std::vector<std::string>&& arg) {
                o->presencePinNames(std::move(arg));
            }));
    }
    catch (const std::exception&)
    {
        o->get_bus().set_current_exception(std::current_exception());
        return 1;
    }
}

namespace details
{
namespace GPIODeviceDetect
{
static const auto _property_PresencePinNames = utility::tuple_to_array(
    message::types::type_id<std::vector<std::string>>());
}
} // namespace details

auto GPIODeviceDetect::presencePinValues() const -> std::vector<uint64_t>
{
    return _presencePinValues;
}

int GPIODeviceDetect::_callback_get_PresencePinValues(
    sd_bus* /*bus*/, const char* /*path*/, const char* /*interface*/,
    const char* /*property*/, sd_bus_message* reply, void* context,
    sd_bus_error* error)
{
    auto o = static_cast<GPIODeviceDetect*>(context);

    try
    {
        return sdbusplus::sdbuspp::property_callback(
            reply, o->get_bus().getInterface(), error,
            std::function([=]() { return o->presencePinValues(); }));
    }
    catch (const std::exception&)
    {
        o->get_bus().set_current_exception(std::current_exception());
        return 1;
    }
}

auto GPIODeviceDetect::presencePinValues(std::vector<uint64_t> value,
                                         bool skipSignal)
    -> std::vector<uint64_t>
{
    if (_presencePinValues != value)
    {
        _presencePinValues = value;
        if (!skipSignal)
        {
            _xyz_openbmc_project_configuration_GPIODeviceDetect_interface
                .property_changed("PresencePinValues");
        }
    }

    return _presencePinValues;
}

auto GPIODeviceDetect::presencePinValues(std::vector<uint64_t> val)
    -> std::vector<uint64_t>
{
    return presencePinValues(val, false);
}

int GPIODeviceDetect::_callback_set_PresencePinValues(
    sd_bus* /*bus*/, const char* /*path*/, const char* /*interface*/,
    const char* /*property*/, sd_bus_message* value, void* context,
    sd_bus_error* error)
{
    auto o = static_cast<GPIODeviceDetect*>(context);

    try
    {
        return sdbusplus::sdbuspp::property_callback(
            value, o->get_bus().getInterface(), error,
            std::function([=](std::vector<uint64_t>&& arg) {
                o->presencePinValues(std::move(arg));
            }));
    }
    catch (const std::exception&)
    {
        o->get_bus().set_current_exception(std::current_exception());
        return 1;
    }
}

namespace details
{
namespace GPIODeviceDetect
{
static const auto _property_PresencePinValues =
    utility::tuple_to_array(message::types::type_id<std::vector<uint64_t>>());
}
} // namespace details

void GPIODeviceDetect::setPropertyByName(
    const std::string& _name, const PropertiesVariant& val, bool skipSignal)
{
    if (_name == "Name")
    {
        auto& v = std::get<std::string>(val);
        name(v, skipSignal);
        return;
    }
    if (_name == "PresencePinNames")
    {
        auto& v = std::get<std::vector<std::string>>(val);
        presencePinNames(v, skipSignal);
        return;
    }
    if (_name == "PresencePinValues")
    {
        auto& v = std::get<std::vector<uint64_t>>(val);
        presencePinValues(v, skipSignal);
        return;
    }
}

auto GPIODeviceDetect::getPropertyByName(const std::string& _name)
    -> PropertiesVariant
{
    if (_name == "Name")
    {
        return name();
    }
    if (_name == "PresencePinNames")
    {
        return presencePinNames();
    }
    if (_name == "PresencePinValues")
    {
        return presencePinValues();
    }

    return PropertiesVariant();
}

const vtable_t GPIODeviceDetect::_vtable[] = {
    vtable::start(),

    vtable::property("Name", details::GPIODeviceDetect::_property_Name.data(),
                     _callback_get_Name, _callback_set_Name,
                     vtable::property_::emits_change),

    vtable::property(
        "PresencePinNames",
        details::GPIODeviceDetect::_property_PresencePinNames.data(),
        _callback_get_PresencePinNames, _callback_set_PresencePinNames,
        vtable::property_::emits_change),

    vtable::property(
        "PresencePinValues",
        details::GPIODeviceDetect::_property_PresencePinValues.data(),
        _callback_get_PresencePinValues, _callback_set_PresencePinValues,
        vtable::property_::emits_change),

    vtable::end()};

} // namespace sdbusplus::server::xyz::openbmc_project::configuration
// NOLINTEND
