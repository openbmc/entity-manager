#pragma once

#include "configuration.hpp"

#include <boost/container/flat_map.hpp>
#include <nlohmann/json.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <iostream>

class SystemMapper;

class InventoryManager
{
  public:
    InventoryManager(boost::asio::io_context& io,
                     std::shared_ptr<sdbusplus::asio::connection>& systemBus,
                     std::shared_ptr<sdbusplus::asio::object_server>& objServer,
                     SystemMapper& mapper);

    SystemMapper& systemMapper;
    boost::container::flat_map<
        std::string,
        std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>>
        inventory;

    static void tryIfaceInitialize(
        std::shared_ptr<sdbusplus::asio::dbus_interface>& iface);

    std::shared_ptr<sdbusplus::asio::dbus_interface> createInterface(
        const std::string& path, const std::string& interface,
        const std::string& parent, bool checkNull = false);

    std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>&
        getDeviceInterfaces(const nlohmann::json& device);

    void createAddObjectMethod(const std::string& jsonPointerPath,
                               const std::string& path,
                               const std::string& board);

    void createDeleteObjectMethod(
        const std::string& jsonPointerPath,
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& iface);

    void populateInterfacePropertyFromJson(
        const std::string& path, const nlohmann::json& key,
        const nlohmann::json& value, nlohmann::json::value_t type,
        std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
        sdbusplus::asio::PropertyPermission permission);

    void populateInterfaceFromJson(
        const std::string& jsonPointerPath,
        std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
        nlohmann::json& dict,
        sdbusplus::asio::PropertyPermission permission =
            sdbusplus::asio::PropertyPermission::readOnly);

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

  private:
    boost::asio::io_context& io;
    std::shared_ptr<sdbusplus::asio::connection> systemBus;
    std::shared_ptr<sdbusplus::asio::object_server> objServer;
};
