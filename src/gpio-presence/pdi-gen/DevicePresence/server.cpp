// NOLINTBEGIN
#include "server.hpp"

#include <sdbusplus/sdbus.hpp>
#include <sdbusplus/sdbuspp_support/server.hpp>
#include <sdbusplus/server.hpp>

#include <exception>
#include <map>
#include <string>
#include <tuple>

namespace sdbusplus::server::xyz::openbmc_project::inventory::source
{

auto DevicePresence::name() const -> std::string
{
    return _name;
}

int DevicePresence::_callback_get_Name(
    sd_bus* /*bus*/, const char* /*path*/, const char* /*interface*/,
    const char* /*property*/, sd_bus_message* reply, void* context,
    sd_bus_error* error)
{
    auto o = static_cast<DevicePresence*>(context);

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

auto DevicePresence::name(std::string value, bool skipSignal) -> std::string
{
    if (_name != value)
    {
        _name = value;
        if (!skipSignal)
        {
            _xyz_openbmc_project_inventory_source_DevicePresence_interface
                .property_changed("Name");
        }
    }

    return _name;
}

auto DevicePresence::name(std::string val) -> std::string
{
    return name(val, false);
}

int DevicePresence::_callback_set_Name(
    sd_bus* /*bus*/, const char* /*path*/, const char* /*interface*/,
    const char* /*property*/, sd_bus_message* value, void* context,
    sd_bus_error* error)
{
    auto o = static_cast<DevicePresence*>(context);

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
namespace DevicePresence
{
static const auto _property_Name =
    utility::tuple_to_array(message::types::type_id<std::string>());
}
} // namespace details

void DevicePresence::setPropertyByName(
    const std::string& _name, const PropertiesVariant& val, bool skipSignal)
{
    if (_name == "Name")
    {
        auto& v = std::get<std::string>(val);
        name(v, skipSignal);
        return;
    }
}

auto DevicePresence::getPropertyByName(const std::string& _name)
    -> PropertiesVariant
{
    if (_name == "Name")
    {
        return name();
    }

    return PropertiesVariant();
}

const vtable_t DevicePresence::_vtable[] = {
    vtable::start(),

    vtable::property("Name", details::DevicePresence::_property_Name.data(),
                     _callback_get_Name, _callback_set_Name,
                     vtable::property_::emits_change),

    vtable::end()};

} // namespace sdbusplus::server::xyz::openbmc_project::inventory::source
// NOLINTEND
