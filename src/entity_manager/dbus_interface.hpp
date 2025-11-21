#pragma once

#include "config_cache.hpp"
#include "configuration.hpp"

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
                    const std::filesystem::path& schemaDirectory,
                    const std::shared_ptr<ConfigCache>& configCache);

    std::shared_ptr<sdbusplus::asio::dbus_interface> createInterface(
        const std::string& path, const std::string& interface,
        const std::string& parent, bool checkNull = false);

    std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>&
        getDeviceInterfaces(const nlohmann::json& device);

    void createAddObjectMethod(const std::string& jsonPointerPath,
                               const std::string& path,
                               nlohmann::json& systemConfiguration,
                               const std::string& board);

    void populateInterfaceFromJson(
        nlohmann::json& systemConfiguration, const std::string& jsonPointerPath,
        std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
        nlohmann::json& dict,
        sdbusplus::asio::PropertyPermission permission =
            sdbusplus::asio::PropertyPermission::readOnly);

    void populateInterfacePropertyFromJson(
        nlohmann::json& systemConfiguration, const std::string& path,
        const std::string& key, const nlohmann::json& value,
        nlohmann::json::value_t type,
        std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
        sdbusplus::asio::PropertyPermission permission);

    void createDeleteObjectMethod(
        const std::string& jsonPointerPath,
        const std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
        nlohmann::json& systemConfiguration);

  private:
    void addObject(
        const std::flat_map<std::string, JsonVariantType, std::less<>>& data,
        nlohmann::json& systemConfiguration, const std::string& jsonPointerPath,
        const std::string& path, const std::string& board);

    // @brief: same as 'addObject', but operates on json
    void addObjectJson(nlohmann::json& newData,
                       nlohmann::json& systemConfiguration,
                       const std::string& jsonPointerPath,
                       const std::string& path, const std::string& board);

    boost::asio::io_context& io;
    sdbusplus::asio::object_server& objServer;

    std::flat_map<std::string,
                  std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>,
                  std::less<>>
        inventory;

    const std::filesystem::path schemaDirectory;

    // some DBus methods have effects on our configuration,
    // store a shared_ptr here to avoid passing it around everywhere
    std::shared_ptr<ConfigCache> configCache;
};

void tryIfaceInitialize(
    std::shared_ptr<sdbusplus::asio::dbus_interface>& iface);

template <typename PropertyType>
void addArrayToDbus(const std::string& name, const nlohmann::json& array,
                    sdbusplus::asio::dbus_interface* iface,
                    sdbusplus::asio::PropertyPermission permission,
                    nlohmann::json& systemConfiguration,
                    const std::string& jsonPointerString,
                    std::shared_ptr<ConfigCache> configCache)
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
            [&systemConfiguration, &configCache,
             jsonPointerString{std::string(jsonPointerString)}](
                const std::vector<PropertyType>& newVal,
                std::vector<PropertyType>& val) {
                val = newVal;
                if (!setJsonFromPointer(jsonPointerString, val,
                                        systemConfiguration))
                {
                    lg2::error("error setting json field");
                    return -1;
                }
                if (!configCache->writeJsonFiles(systemConfiguration))
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
                 nlohmann::json& systemConfiguration,
                 const std::string& jsonPointerString,
                 sdbusplus::asio::PropertyPermission permission,
                 std::shared_ptr<ConfigCache> configCache)
{
    if (permission == sdbusplus::asio::PropertyPermission::readOnly)
    {
        iface->register_property(name, value);
        return;
    }
    iface->register_property(
        name, value,
        [&systemConfiguration, &configCache,
         jsonPointerString{std::string(jsonPointerString)}](
            const PropertyType& newVal, PropertyType& val) {
            val = newVal;
            if (!setJsonFromPointer(jsonPointerString, val,
                                    systemConfiguration))
            {
                lg2::error("error setting json field");
                return -1;
            }
            if (!configCache->writeJsonFiles(systemConfiguration))
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
                    nlohmann::json& systemConfiguration,
                    const std::string& path,
                    const std::shared_ptr<ConfigCache>& configCache)
{
    if (value.is_array())
    {
        addArrayToDbus<PropertyType>(key, value, &iface, permission,
                                     systemConfiguration, path, configCache);
    }
    else
    {
        addProperty(key, value.get<PropertyType>(), &iface, systemConfiguration,
                    path, sdbusplus::asio::PropertyPermission::readOnly,
                    configCache);
    }
}

} // namespace dbus_interface
