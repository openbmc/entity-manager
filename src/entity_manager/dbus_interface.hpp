#pragma once

#include "configuration.hpp"

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>
#include <vector>

namespace dbus_interface
{

class EMDBusInterface
{
  public:
    std::shared_ptr<sdbusplus::asio::dbus_interface> createInterface(
        sdbusplus::asio::object_server& objServer, const std::string& path,
        const std::string& interface, const std::string& parent,
        bool checkNull = false);

    std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>&
        getDeviceInterfaces(const nlohmann::json& device);

    void createAddObjectMethod(
        boost::asio::io_context& io, const std::string& jsonPointerPath,
        const std::string& path, nlohmann::json& systemConfiguration,
        sdbusplus::asio::object_server& objServer, const std::string& board);

  private:
    boost::container::flat_map<
        std::string,
        std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>>
        inventory;
};

void tryIfaceInitialize(
    std::shared_ptr<sdbusplus::asio::dbus_interface>& iface);

template <typename PropertyType>
void addArrayToDbus(const std::string& name, const nlohmann::json& array,
                    sdbusplus::asio::dbus_interface* iface,
                    sdbusplus::asio::PropertyPermission permission,
                    nlohmann::json& systemConfiguration,
                    const std::string& jsonPointerString)
{
    std::vector<PropertyType> values;
    for (const auto& property : array)
    {
        auto ptr = property.get_ptr<const PropertyType*>();
        if (ptr != nullptr)
        {
            values.emplace_back(*ptr);
        }
    }

    if (permission == sdbusplus::asio::PropertyPermission::readOnly)
    {
        iface->register_property(name, values);
    }
    else
    {
        iface->register_property(
            name, values,
            [&systemConfiguration,
             jsonPointerString{std::string(jsonPointerString)}](
                const std::vector<PropertyType>& newVal,
                std::vector<PropertyType>& val) {
                val = newVal;
                if (!setJsonFromPointer(jsonPointerString, val,
                                        systemConfiguration))
                {
                    std::cerr << "error setting json field\n";
                    return -1;
                }
                if (!writeJsonFiles(systemConfiguration))
                {
                    std::cerr << "error setting json file\n";
                    return -1;
                }
                return 1;
            });
    }
}

template <typename PropertyType>
void addProperty(const std::string& name, const PropertyType& value,
                 sdbusplus::asio::dbus_interface* iface,
                 nlohmann::json& systemConfiguration,
                 const std::string& jsonPointerString,
                 sdbusplus::asio::PropertyPermission permission)
{
    if (permission == sdbusplus::asio::PropertyPermission::readOnly)
    {
        iface->register_property(name, value);
        return;
    }
    iface->register_property(
        name, value,
        [&systemConfiguration,
         jsonPointerString{std::string(jsonPointerString)}](
            const PropertyType& newVal, PropertyType& val) {
            val = newVal;
            if (!setJsonFromPointer(jsonPointerString, val,
                                    systemConfiguration))
            {
                std::cerr << "error setting json field\n";
                return -1;
            }
            if (!writeJsonFiles(systemConfiguration))
            {
                std::cerr << "error setting json file\n";
                return -1;
            }
            return 1;
        });
}

template <typename PropertyType, typename SinglePropertyType = PropertyType>
void addValueToDBus(const std::string& key, const nlohmann::json& value,
                    sdbusplus::asio::dbus_interface& iface,
                    sdbusplus::asio::PropertyPermission permission,
                    nlohmann::json& systemConfiguration,
                    const std::string& path)
{
    if (value.is_array())
    {
        addArrayToDbus<PropertyType>(key, value, &iface, permission,
                                     systemConfiguration, path);
    }
    else
    {
        addProperty(key, value.get<SinglePropertyType>(), &iface,
                    systemConfiguration, path,
                    sdbusplus::asio::PropertyPermission::readOnly);
    }
}

void createDeleteObjectMethod(
    const std::string& jsonPointerPath,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    sdbusplus::asio::object_server& objServer,
    nlohmann::json& systemConfiguration, boost::asio::io_context& io);

void populateInterfaceFromJson(
    boost::asio::io_context& io, nlohmann::json& systemConfiguration,
    const std::string& jsonPointerPath,
    std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    nlohmann::json& dict, sdbusplus::asio::object_server& objServer,
    sdbusplus::asio::PropertyPermission permission =
        sdbusplus::asio::PropertyPermission::readOnly);

} // namespace dbus_interface
