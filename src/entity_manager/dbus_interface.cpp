#include "dbus_interface.hpp"

#include "perform_probe.hpp"
#include "phosphor-logging/lg2.hpp"
#include "utils.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/container/flat_map.hpp>

#include <fstream>
#include <regex>
#include <string>
#include <vector>

PHOSPHOR_LOG2_USING;

using JsonVariantType =
    std::variant<std::vector<std::string>, std::vector<double>, std::string,
                 int64_t, uint64_t, double, int32_t, uint32_t, int16_t,
                 uint16_t, uint8_t, bool>;

namespace dbus_interface
{

const std::regex illegalDbusPathRegex("[^A-Za-z0-9_.]");
const std::regex illegalDbusMemberRegex("[^A-Za-z0-9_]");

EMDBusInterface::EMDBusInterface(boost::asio::io_context& io,
                                 sdbusplus::asio::object_server& objServer) :
    io(io), objServer(objServer)
{}

void tryIfaceInitialize(std::shared_ptr<sdbusplus::asio::dbus_interface>& iface)
{
    try
    {
        iface->initialize();
    }
    catch (std::exception& e)
    {
        std::cerr << "Unable to initialize dbus interface : " << e.what()
                  << "\n"
                  << "object Path : " << iface->get_object_path() << "\n"
                  << "interface name : " << iface->get_interface_name() << "\n";
    }
}

std::shared_ptr<sdbusplus::asio::dbus_interface>
    EMDBusInterface::createInterface(const std::string& path,
                                     const std::string& interface,
                                     const std::string& parent, bool checkNull)
{
    // on first add we have no reason to check for null before add, as there
    // won't be any. For dynamically added interfaces, we check for null so that
    // a constant delete/add will not create a memory leak

    auto ptr = objServer.add_interface(path, interface);
    auto& dataVector = inventory[parent];
    if (checkNull)
    {
        auto it = std::find_if(dataVector.begin(), dataVector.end(),
                               [](const auto& p) { return p.expired(); });
        if (it != dataVector.end())
        {
            *it = ptr;
            return ptr;
        }
    }
    dataVector.emplace_back(ptr);
    return ptr;
}

void EMDBusInterface::createDeleteObjectMethod(
    const std::string& jsonPointerPath,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    nlohmann::json& systemConfiguration)
{
    std::weak_ptr<sdbusplus::asio::dbus_interface> interface = iface;
    iface->register_method(
        "Delete", [this, &systemConfiguration, interface,
                   jsonPointerPath{std::string(jsonPointerPath)}]() {
            std::shared_ptr<sdbusplus::asio::dbus_interface> dbusInterface =
                interface.lock();
            if (!dbusInterface)
            {
                // this technically can't happen as the pointer is pointing to
                // us
                throw DBusInternalError();
            }
            nlohmann::json::json_pointer ptr(jsonPointerPath);
            systemConfiguration[ptr] = nullptr;

            // todo(james): dig through sdbusplus to find out why we can't
            // delete it in a method call
            boost::asio::post(io, [dbusInterface, this]() mutable {
                objServer.remove_interface(dbusInterface);
            });

            if (!writeJsonFiles(systemConfiguration))
            {
                std::cerr << "error setting json file\n";
                throw DBusInternalError();
            }
        });
}

static bool checkArrayElementsSameType(nlohmann::json& value)
{
    nlohmann::json::array_t* arr = value.get_ptr<nlohmann::json::array_t*>();
    if (arr == nullptr)
    {
        return false;
    }

    if (arr->empty())
    {
        return true;
    }

    nlohmann::json::value_t firstType = value[0].type();
    return std::ranges::all_of(value, [firstType](const nlohmann::json& el) {
        return el.type() == firstType;
    });
}

static nlohmann::json::value_t getDBusType(
    const nlohmann::json& value, nlohmann::json::value_t type,
    sdbusplus::asio::PropertyPermission permission)
{
    const bool array = value.type() == nlohmann::json::value_t::array;

    if (permission == sdbusplus::asio::PropertyPermission::readWrite)
    {
        // all setable numbers are doubles as it is difficult to always
        // create a configuration file with all whole numbers as decimals
        // i.e. 1.0
        if (array)
        {
            if (value[0].is_number())
            {
                return nlohmann::json::value_t::number_float;
            }
        }
        else if (value.is_number())
        {
            return nlohmann::json::value_t::number_float;
        }
    }

    return type;
}

static void populateInterfacePropertyFromJson(
    nlohmann::json& systemConfiguration, const std::string& path,
    const nlohmann::json& key, const nlohmann::json& value,
    nlohmann::json::value_t type,
    std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    sdbusplus::asio::PropertyPermission permission)
{
    const auto modifiedType = getDBusType(value, type, permission);

    switch (modifiedType)
    {
        case (nlohmann::json::value_t::boolean):
        {
            // todo: array of bool isn't detected correctly by
            // sdbusplus, change it to numbers
            addValueToDBus<uint64_t, bool>(key, value, *iface, permission,
                                           systemConfiguration, path);
            break;
        }
        case (nlohmann::json::value_t::number_integer):
        {
            addValueToDBus<int64_t>(key, value, *iface, permission,
                                    systemConfiguration, path);
            break;
        }
        case (nlohmann::json::value_t::number_unsigned):
        {
            addValueToDBus<uint64_t>(key, value, *iface, permission,
                                     systemConfiguration, path);
            break;
        }
        case (nlohmann::json::value_t::number_float):
        {
            addValueToDBus<double>(key, value, *iface, permission,
                                   systemConfiguration, path);
            break;
        }
        case (nlohmann::json::value_t::string):
        {
            addValueToDBus<std::string>(key, value, *iface, permission,
                                        systemConfiguration, path);
            break;
        }
        default:
        {
            std::cerr << "Unexpected json type in system configuration " << key
                      << ": " << value.type_name() << "\n";
            break;
        }
    }
}

void EMDBusInterface::populateIntfPDICompatObject(
    nlohmann::json& systemConfiguration, const std::string& jsonPointerPath,
    std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    const std::string& key, nlohmann::json& value,
    const std::string& boardNameOrig,
    const config_type_tree::ConfigTypeNode& ctn,
    sdbusplus::asio::PropertyPermission permission, size_t depth)
{
    const std::string ifacePathAlt =
        std::format("{}/{}", iface->get_object_path(), key);
    const std::string ifaceNameAlt =
        std::format("xyz.openbmc_project.Configuration.{}", ctn.type);
    std::shared_ptr<sdbusplus::asio::dbus_interface> objectIfaceAlt =
        createInterface(ifacePathAlt, ifaceNameAlt, boardNameOrig);
    populateIntfPDICompat(systemConfiguration, jsonPointerPath, objectIfaceAlt,
                          value, boardNameOrig, ctn, permission, depth + 1);
}

void EMDBusInterface::populateIntfPDICompatArray(
    nlohmann::json& systemConfiguration, const std::string& jsonPointerPath,
    std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    const std::string& propertyName, nlohmann::json& value,
    const std::string& boardNameOrig,
    const config_type_tree::ConfigTypeNode& ctn,
    sdbusplus::asio::PropertyPermission permission, size_t depth)
{
    for (const auto& [index, v] : value.items())
    {
        const std::string ifacePathAlt = std::format(
            "{}/{}/{}", iface->get_object_path(), propertyName, index);
        const std::string ifaceNameAlt =
            std::format("xyz.openbmc_project.Configuration.{}", ctn.type);
        std::shared_ptr<sdbusplus::asio::dbus_interface> objectIfaceAlt =
            createInterface(ifacePathAlt, ifaceNameAlt, boardNameOrig);
        populateIntfPDICompat(systemConfiguration, jsonPointerPath,
                              objectIfaceAlt, v, boardNameOrig, ctn, permission,
                              depth + 1);
    }
}

void EMDBusInterface::populateIntfPDICompat(
    nlohmann::json& systemConfiguration, const std::string& jsonPointerPath,
    std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    nlohmann::json& dict, const std::string& boardNameOrig,
    const config_type_tree::ConfigTypeNode& ctn,
    sdbusplus::asio::PropertyPermission permission, size_t depth)
{
    if constexpr (!PDI_COMPATIBLE_DBUS)
    {
        return;
    }

    const size_t maxDepth = 3;

    if (depth >= maxDepth)
    {
        lg2::error("maximum depth exceeded for object path, skipping");
        return;
    }

    for (const auto& [key, value] : dict.items())
    {
        auto type = value.type();
        if (value.type() == nlohmann::json::value_t::array)
        {
            if (value.empty())
            {
                continue;
            }
            if (!checkArrayElementsSameType(value))
            {
                std::cerr << "dbus format error" << value << "\n";
                continue;
            }
        }
        if (type == nlohmann::json::value_t::object)
        {
            if (!ctn.properties.contains(key))
            {
                continue;
            }

            populateIntfPDICompatObject(
                systemConfiguration, jsonPointerPath, iface, key, value,
                boardNameOrig, ctn.properties.at(key), permission, depth);
            continue;
        }
        if (type == nlohmann::json::value_t::array && !value.empty() &&
            value[0].type() == nlohmann::json::value_t::object &&
            ctn.properties.contains(key))
        {
            populateIntfPDICompatArray(
                systemConfiguration, jsonPointerPath, iface, key, value,
                boardNameOrig, ctn.properties.at(key), permission, depth);
            continue;
        }

        std::string path = jsonPointerPath;
        path.append("/").append(key);

        populateInterfacePropertyFromJson(systemConfiguration, path, key, value,
                                          type, iface, permission);
    }
    if (permission == sdbusplus::asio::PropertyPermission::readWrite)
    {
        createDeleteObjectMethod(jsonPointerPath, iface, systemConfiguration);
    }
    tryIfaceInitialize(iface);
}

// adds simple json types to interface's properties
void EMDBusInterface::populateInterfaceFromJson(
    nlohmann::json& systemConfiguration, const std::string& jsonPointerPath,
    std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    nlohmann::json& dict, sdbusplus::asio::PropertyPermission permission)
{
    for (const auto& [key, value] : dict.items())
    {
        auto type = value.type();
        if (value.type() == nlohmann::json::value_t::array)
        {
            if (value.empty())
            {
                continue;
            }
            type = value[0].type();
            if (!checkArrayElementsSameType(value))
            {
                std::cerr << "dbus format error" << value << "\n";
                continue;
            }
        }
        if (type == nlohmann::json::value_t::object)
        {
            continue; // handled elsewhere
        }

        std::string path = jsonPointerPath;
        path.append("/").append(key);

        populateInterfacePropertyFromJson(systemConfiguration, path, key, value,
                                          type, iface, permission);
    }
    if (permission == sdbusplus::asio::PropertyPermission::readWrite)
    {
        createDeleteObjectMethod(jsonPointerPath, iface, systemConfiguration);
    }
    tryIfaceInitialize(iface);
}

void EMDBusInterface::createAddObjectMethod(
    const std::string& jsonPointerPath, const std::string& path,
    nlohmann::json& systemConfiguration, const std::string& board)
{
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        createInterface(path, "xyz.openbmc_project.AddObject", board);

    iface->register_method(
        "AddObject",
        [&systemConfiguration, jsonPointerPath{std::string(jsonPointerPath)},
         path{std::string(path)}, board,
         this](const boost::container::flat_map<std::string, JsonVariantType>&
                   data) {
            nlohmann::json::json_pointer ptr(jsonPointerPath);
            nlohmann::json& base = systemConfiguration[ptr];
            auto findExposes = base.find("Exposes");

            if (findExposes == base.end())
            {
                throw std::invalid_argument("Entity must have children.");
            }

            // this will throw invalid-argument to sdbusplus if invalid json
            nlohmann::json newData{};
            for (const auto& item : data)
            {
                nlohmann::json& newJson = newData[item.first];
                std::visit(
                    [&newJson](auto&& val) {
                        newJson = std::forward<decltype(val)>(val);
                    },
                    item.second);
            }

            auto findName = newData.find("Name");
            auto findType = newData.find("Type");
            if (findName == newData.end() || findType == newData.end())
            {
                throw std::invalid_argument("AddObject missing Name or Type");
            }
            const std::string* type = findType->get_ptr<const std::string*>();
            const std::string* name = findName->get_ptr<const std::string*>();
            if (type == nullptr || name == nullptr)
            {
                throw std::invalid_argument("Type and Name must be a string.");
            }

            bool foundNull = false;
            size_t lastIndex = 0;
            // we add in the "exposes"
            for (const auto& expose : *findExposes)
            {
                if (expose.is_null())
                {
                    foundNull = true;
                    continue;
                }

                if (expose["Name"] == *name && expose["Type"] == *type)
                {
                    throw std::invalid_argument(
                        "Field already in JSON, not adding");
                }

                if (foundNull)
                {
                    continue;
                }

                lastIndex++;
            }

            std::ifstream schemaFile(std::string(schemaDirectory) + "/" +
                                     boost::to_lower_copy(*type) + ".json");
            // todo(james) we might want to also make a list of 'can add'
            // interfaces but for now I think the assumption if there is a
            // schema avaliable that it is allowed to update is fine
            if (!schemaFile.good())
            {
                throw std::invalid_argument(
                    "No schema avaliable, cannot validate.");
            }
            nlohmann::json schema =
                nlohmann::json::parse(schemaFile, nullptr, false, true);
            if (schema.is_discarded())
            {
                std::cerr << "Schema not legal" << *type << ".json\n";
                throw DBusInternalError();
            }
            if (!validateJson(schema, newData))
            {
                throw std::invalid_argument("Data does not match schema");
            }
            if (foundNull)
            {
                findExposes->at(lastIndex) = newData;
            }
            else
            {
                findExposes->push_back(newData);
            }
            if (!writeJsonFiles(systemConfiguration))
            {
                std::cerr << "Error writing json files\n";
            }
            std::string dbusName = *name;

            std::regex_replace(dbusName.begin(), dbusName.begin(),
                               dbusName.end(), illegalDbusMemberRegex, "_");

            std::shared_ptr<sdbusplus::asio::dbus_interface> interface =
                createInterface(path + "/" + dbusName,
                                "xyz.openbmc_project.Configuration." + *type,
                                board, true);
            // permission is read-write, as since we just created it, must be
            // runtime modifiable
            populateInterfaceFromJson(
                systemConfiguration,
                jsonPointerPath + "/Exposes/" + std::to_string(lastIndex),
                interface, newData,
                sdbusplus::asio::PropertyPermission::readWrite);
        });
    tryIfaceInitialize(iface);
}

std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>&
    EMDBusInterface::getDeviceInterfaces(const nlohmann::json& device)
{
    return inventory[device["Name"].get<std::string>()];
}

} // namespace dbus_interface
