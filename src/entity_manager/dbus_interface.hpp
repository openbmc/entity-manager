#pragma once

#include "configuration.hpp"

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>

#include <flat_map>
#include <iostream>
#include <map>
#include <string>
#include <variant>
#include <vector>

using foundProbeData = std::map<std::string, std::string>;
extern std::map<std::string, foundProbeData>
   mapFoundData; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

inline constexpr const char* fruConn = "xyz.openbmc_project.FruDevice";
inline constexpr const char* fruIntf = "xyz.openbmc_project.FruDevice";

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
bool persistAssetTag(const PropertyType& newVal,
                     const std::string& jsonPointerString)
{
    /*Strip last component (e.g., "/AssetTag") */
    std::size_t found = jsonPointerString.find_last_of('/');
    std::string mapKey = (found != std::string::npos)
                             ? jsonPointerString.substr(0, found)
                             : jsonPointerString;

    /* Get FRU path dynamically from mapFoundData */
    std::string fruObjPath;
    auto it = mapFoundData.find(mapKey);
    if (it != mapFoundData.end())
    {
        auto foundIt = it->second.find("foundPath");
        if (foundIt != it->second.end())
        {
            fruObjPath = foundIt->second;
            lg2::info("Resolved FRU path from mapFoundData: {P}", "P",
                      fruObjPath);
        }
        else
        {
            lg2::error("No 'foundPath' entry in mapFoundData for key={K}", "K",
                       mapKey);
            return false; // Cannot proceed without dynamic FRU path
        }
    }
    else
    {
        lg2::error("mapFoundData key not found: {K}", "K", mapKey);
        return false; // Cannot proceed without dynamic FRU path
    }

    /* Call D-Bus FRU UpdateFruField */
    try
    {
        auto bus = sdbusplus::bus::new_system();
        auto method = bus.new_method_call(
            "xyz.openbmc_project.FruDevice", fruObjPath.c_str(),
            "xyz.openbmc_project.FruDevice", "UpdateFruField");

        method.append(std::string("BOARD_INFO_AM1"),
                      std::string(newVal).substr(0, 16));

        auto reply = bus.call(method);
        bool success = false;
        reply.read(success);

        if (!success)
        {
            lg2::warning("FRU does not support updating AssetTag ");
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        lg2::warning("FRU update skipped err={E}", "E", e.what());
    }

    lg2::info("persistAssetTag SUCCESS value={V}", "V", newVal);
    return true;
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
        [name, &systemConfiguration, jsonPointerString](
            const PropertyType& newVal, PropertyType& val) -> int {
            if constexpr (std::is_same_v<PropertyType, std::string>)
            {
                if (name == "AssetTag")
                {
                    if (!persistAssetTag(newVal, jsonPointerString))
                    {
                        lg2::error("persistAssetTag FAILED");
                        return -1;
                    }
                }
            }

            val = newVal;

            if (!setJsonFromPointer(jsonPointerString, val,
                                    systemConfiguration))
            {
                lg2::error("setJsonFromPointer FAILED");
                return -1;
            }

            if (!writeJsonFiles(systemConfiguration))
            {
                lg2::error("writeJsonFiles FAILED");
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
                    const std::string& path)
{
    if (value.is_array())
    {
        addArrayToDbus<PropertyType>(key, value, &iface, permission,
                                     systemConfiguration, path);
    }
    else
    {
        addProperty(key, value.get<PropertyType>(), &iface, systemConfiguration,
                    path, permission);
    }
}

} // namespace dbus_interface
