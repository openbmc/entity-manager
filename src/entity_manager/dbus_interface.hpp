#pragma once

#include "config_pointer.hpp"
#include "configuration.hpp"
#include "system_configuration.hpp"

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <flat_map>
#include <vector>

namespace dbus_interface
{

using JsonVariantType =
    std::variant<std::vector<std::string>, std::vector<double>, std::string,
                 int64_t, uint64_t, double, int32_t, uint32_t, int16_t,
                 uint16_t, uint8_t, bool>;

class EMDBusInterface
{
  public:
    EMDBusInterface(boost::asio::io_context& io,
                    sdbusplus::asio::object_server& objServer,
                    const std::filesystem::path& schemaDirectory);

    std::shared_ptr<sdbusplus::asio::dbus_interface> createInterface(
        const std::string& path, const std::string& interface,
        const std::string& parent, bool checkNull = false);

    std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>&
        getDeviceInterfaces(const nlohmann::json& device);

    void createAddObjectMethod(const std::string& boardId,
                               const std::string& path,
                               SystemConfiguration& systemConfiguration,
                               const std::string& board);

    void populateInterfaceFromJson(
        SystemConfiguration& systemConfiguration,
        const ConfigPointer& configPtr,
        std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
        const nlohmann::json::object_t& dict,
        sdbusplus::asio::PropertyPermission permission =
            sdbusplus::asio::PropertyPermission::readOnly);

    void createDeleteObjectMethod(
        const ConfigPointer& configPtr,
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
        SystemConfiguration& systemConfiguration);

  private:
    void addObject(
        const std::flat_map<std::string, JsonVariantType, std::less<>>& data,
        SystemConfiguration& systemConfiguration, const std::string& boardId,
        const std::string& path, const std::string& board);

    // @brief: same as 'addObject', but operates on json
    void addObjectJson(nlohmann::json::object_t& newData,
                       SystemConfiguration& systemConfiguration,
                       const std::string& boardId, const std::string& path,
                       const std::string& board);

    boost::asio::io_context& io;
    sdbusplus::asio::object_server& objServer;

    std::flat_map<std::string,
                  std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>,
                  std::less<>>
        inventory;

    const std::filesystem::path schemaDirectory;
};

void tryIfaceInitialize(
    std::shared_ptr<sdbusplus::asio::dbus_interface>& iface);

template <typename PropertyType>
void addArrayToDbus(const std::string& name, const nlohmann::json& array,
                    sdbusplus::asio::dbus_interface* iface,
                    sdbusplus::asio::PropertyPermission permission,
                    SystemConfiguration& systemConfiguration,
                    const ConfigPointer& configPtr)
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
            [&systemConfiguration, configPtr,
             name](const std::vector<PropertyType>& newVal,
                   std::vector<PropertyType>& val) {
                val = newVal;
                if (!setJsonFromPointer(configPtr.withName(name), val,
                                        systemConfiguration))
                {
                    lg2::error("error setting json field");
                    return -1;
                }
                if (!writeJsonFiles(systemConfiguration))
                {
                    lg2::error("error setting json file");
                    return -1;
                }
                return 1;
            });
    }
}

template <typename PropertyType>
void addProperty(const std::string& name, const PropertyType& value,
                 sdbusplus::asio::dbus_interface* iface,
                 SystemConfiguration& systemConfiguration,
                 const ConfigPointer& configPtr,
                 sdbusplus::asio::PropertyPermission permission)
{
    if (permission == sdbusplus::asio::PropertyPermission::readOnly)
    {
        iface->register_property(name, value);
        return;
    }
    iface->register_property(
        name, value,
        [&systemConfiguration, configPtr,
         name](const PropertyType& newVal, PropertyType& val) {
            val = newVal;
            if (!setJsonFromPointer(configPtr.withName(name), val,
                                    systemConfiguration))
            {
                lg2::error("error setting json field");
                return -1;
            }
            if (!writeJsonFiles(systemConfiguration))
            {
                lg2::error("error setting json file");
                return -1;
            }
            return 1;
        });
}

template <typename PropertyType>
void addValueToDBus(const std::string& key, const nlohmann::json& value,
                    sdbusplus::asio::dbus_interface& iface,
                    sdbusplus::asio::PropertyPermission permission,
                    SystemConfiguration& systemConfiguration,
                    const ConfigPointer& configPtr)
{
    if (value.is_array())
    {
        addArrayToDbus<PropertyType>(key, value, &iface, permission,
                                     systemConfiguration, configPtr);
    }
    else
    {
        addProperty(key, value.get<PropertyType>(), &iface, systemConfiguration,
                    configPtr, permission);
    }
}

} // namespace dbus_interface
