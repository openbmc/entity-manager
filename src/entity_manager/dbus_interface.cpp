#include "dbus_interface.hpp"

#include "perform_probe.hpp"
#include "utils.hpp"

#include <phosphor-logging/lg2.hpp>

#include <flat_map>
#include <fstream>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace dbus_interface
{

const std::regex illegalDbusPathRegex("[^A-Za-z0-9_.]");
const std::regex illegalDbusMemberRegex("[^A-Za-z0-9_]");

EMDBusInterface::EMDBusInterface(
    boost::asio::io_context& io, sdbusplus::asio::object_server& objServer,
    const std::filesystem::path& schemaDirectory,
    const std::shared_ptr<ConfigCache>& configCache) :
    io(io), objServer(objServer), schemaDirectory(schemaDirectory),
    configCache(configCache)
{}

void tryIfaceInitialize(std::shared_ptr<sdbusplus::asio::dbus_interface>& iface)
{
    try
    {
        iface->initialize();
    }
    catch (std::exception& e)
    {
        lg2::error(
            "Unable to initialize dbus interface : {ERR} object Path : {PATH} interface name : {INTF}",
            "ERR", e, "PATH", iface->get_object_path(), "INTF",
            iface->get_interface_name());
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
    auto& dataVector = inventory.try_emplace(parent).first->second;
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

            if (!configCache->writeJsonFiles(systemConfiguration))
            {
                lg2::error("error setting json file");
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

void EMDBusInterface::populateInterfacePropertyFromJson(
    nlohmann::json& systemConfiguration, const std::string& path,
    const std::string& key, const nlohmann::json& value,
    nlohmann::json::value_t type,
    std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    sdbusplus::asio::PropertyPermission permission)
{
    const auto modifiedType = getDBusType(value, type, permission);

    switch (modifiedType)
    {
        case (nlohmann::json::value_t::boolean):
        {
            addValueToDBus<bool>(key, value, *iface, permission,
                                 systemConfiguration, path, configCache);
            break;
        }
        case (nlohmann::json::value_t::number_integer):
        {
            addValueToDBus<int64_t>(key, value, *iface, permission,
                                    systemConfiguration, path, configCache);
            break;
        }
        case (nlohmann::json::value_t::number_unsigned):
        {
            addValueToDBus<uint64_t>(key, value, *iface, permission,
                                     systemConfiguration, path, configCache);
            break;
        }
        case (nlohmann::json::value_t::number_float):
        {
            addValueToDBus<double>(key, value, *iface, permission,
                                   systemConfiguration, path, configCache);
            break;
        }
        case (nlohmann::json::value_t::string):
        {
            addValueToDBus<std::string>(key, value, *iface, permission,
                                        systemConfiguration, path, configCache);
            break;
        }
        default:
        {
            lg2::error(
                "Unexpected json type in system configuration {KEY}: {VALUE}",
                "KEY", key, "VALUE", value.type_name());
            break;
        }
    }
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
                lg2::error("dbus format error {VALUE}", "VALUE", value);
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

// @brief: throws on error
static void addObjectRuntimeValidateJson(
    const nlohmann::json& newData, const std::string* type,
    const std::filesystem::path& schemaDirectory)
{
    if constexpr (!ENABLE_RUNTIME_VALIDATE_JSON)
    {
        return;
    }

    const std::filesystem::path schemaPath =
        std::filesystem::path(schemaDirectory) / "exposes_record.json";

    std::ifstream schemaFile{schemaPath};

    if (!schemaFile.good())
    {
        throw std::invalid_argument("No schema avaliable, cannot validate.");
    }
    nlohmann::json schema =
        nlohmann::json::parse(schemaFile, nullptr, false, true);
    if (schema.is_discarded())
    {
        lg2::error("Schema not legal: {TYPE}.json", "TYPE", *type);
        throw DBusInternalError();
    }

    if (!validateJson(schema, newData))
    {
        throw std::invalid_argument("Data does not match schema");
    }
}

void EMDBusInterface::addObject(
    const std::flat_map<std::string, JsonVariantType, std::less<>>& data,
    nlohmann::json& systemConfiguration, const std::string& jsonPointerPath,
    const std::string& path, const std::string& board)
{
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

    addObjectJson(newData, systemConfiguration, jsonPointerPath, path, board);
}

void EMDBusInterface::addObjectJson(
    nlohmann::json& newData, nlohmann::json& systemConfiguration,
    const std::string& jsonPointerPath, const std::string& path,
    const std::string& board)
{
    nlohmann::json::json_pointer ptr(jsonPointerPath);
    nlohmann::json& base = systemConfiguration[ptr];
    auto findExposes = base.find("Exposes");
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
            throw std::invalid_argument("Field already in JSON, not adding");
        }

        if (foundNull)
        {
            continue;
        }

        lastIndex++;
    }

    addObjectRuntimeValidateJson(newData, type, schemaDirectory);

    if (foundNull)
    {
        findExposes->at(lastIndex) = newData;
    }
    else
    {
        findExposes->push_back(newData);
    }
    if (!configCache->writeJsonFiles(systemConfiguration))
    {
        lg2::error("Error writing json files");
    }
    std::string dbusName = *name;

    std::regex_replace(dbusName.begin(), dbusName.begin(), dbusName.end(),
                       illegalDbusMemberRegex, "_");

    std::shared_ptr<sdbusplus::asio::dbus_interface> interface =
        createInterface(path + "/" + dbusName,
                        "xyz.openbmc_project.Configuration." + *type, board,
                        true);
    // permission is read-write, as since we just created it, must be
    // runtime modifiable
    populateInterfaceFromJson(
        systemConfiguration,
        jsonPointerPath + "/Exposes/" + std::to_string(lastIndex), interface,
        newData, sdbusplus::asio::PropertyPermission::readWrite);
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
         path{std::string(path)}, board{std::string(board)},
         this](const std::flat_map<std::string, JsonVariantType, std::less<>>&
                   data) {
            addObject(data, systemConfiguration, jsonPointerPath, path, board);
        });
    tryIfaceInitialize(iface);
}

std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>&
    EMDBusInterface::getDeviceInterfaces(const nlohmann::json& device)
{
    return inventory[device["Name"].get<std::string>()];
}

} // namespace dbus_interface
