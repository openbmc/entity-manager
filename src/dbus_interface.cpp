#include "dbus_interface.hpp"

#include "perform_probe.hpp"
#include "utils.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/container/flat_map.hpp>

#include <regex>
#include <string>
#include <vector>

using JsonVariantType =
    std::variant<std::vector<std::string>, std::vector<double>, std::string,
                 int64_t, uint64_t, double, int32_t, uint32_t, int16_t,
                 uint16_t, uint8_t, bool>;

namespace dbus_interface
{

const std::regex illegalDbusPathRegex("[^A-Za-z0-9_.]");
const std::regex illegalDbusMemberRegex("[^A-Za-z0-9_]");

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
// store reference to all interfaces so we can destroy them later
boost::container::flat_map<
    std::string, std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>>
    inventory;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

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

std::shared_ptr<sdbusplus::asio::dbus_interface> createInterface(
    sdbusplus::asio::object_server& objServer, const std::string& path,
    const std::string& interface, const std::string& parent, bool checkNull)
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

void createDeleteObjectMethod(
    const std::string& jsonPointerPath,
    const std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    sdbusplus::asio::object_server& objServer,
    nlohmann::json& systemConfiguration)
{
    std::weak_ptr<sdbusplus::asio::dbus_interface> interface = iface;
    iface->register_method(
        "Delete", [&objServer, &systemConfiguration, interface,
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
            boost::asio::post(io, [&objServer, dbusInterface]() mutable {
                objServer.remove_interface(dbusInterface);
            });

            if (!configuration::writeJsonFiles(systemConfiguration))
            {
                std::cerr << "error setting json file\n";
                throw DBusInternalError();
            }
        });
}

// adds simple json types to interface's properties
void populateInterfaceFromJson(
    nlohmann::json& systemConfiguration, const std::string& jsonPointerPath,
    std::shared_ptr<sdbusplus::asio::dbus_interface>& iface,
    nlohmann::json& dict, sdbusplus::asio::object_server& objServer,
    sdbusplus::asio::PropertyPermission permission)
{
    for (const auto& [key, value] : dict.items())
    {
        auto type = value.type();
        bool array = false;
        if (value.type() == nlohmann::json::value_t::array)
        {
            array = true;
            if (value.empty())
            {
                continue;
            }
            type = value[0].type();
            bool isLegal = true;
            for (const auto& arrayItem : value)
            {
                if (arrayItem.type() != type)
                {
                    isLegal = false;
                    break;
                }
            }
            if (!isLegal)
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
        if (permission == sdbusplus::asio::PropertyPermission::readWrite)
        {
            // all setable numbers are doubles as it is difficult to always
            // create a configuration file with all whole numbers as decimals
            // i.e. 1.0
            if (array)
            {
                if (value[0].is_number())
                {
                    type = nlohmann::json::value_t::number_float;
                }
            }
            else if (value.is_number())
            {
                type = nlohmann::json::value_t::number_float;
            }
        }

        switch (type)
        {
            case (nlohmann::json::value_t::boolean):
            {
                if (array)
                {
                    // todo: array of bool isn't detected correctly by
                    // sdbusplus, change it to numbers
                    addArrayToDbus<uint64_t>(key, value, iface.get(),
                                             permission, systemConfiguration,
                                             path);
                }

                else
                {
                    addProperty(key, value.get<bool>(), iface.get(),
                                systemConfiguration, path, permission);
                }
                break;
            }
            case (nlohmann::json::value_t::number_integer):
            {
                if (array)
                {
                    addArrayToDbus<int64_t>(key, value, iface.get(), permission,
                                            systemConfiguration, path);
                }
                else
                {
                    addProperty(key, value.get<int64_t>(), iface.get(),
                                systemConfiguration, path,
                                sdbusplus::asio::PropertyPermission::readOnly);
                }
                break;
            }
            case (nlohmann::json::value_t::number_unsigned):
            {
                if (array)
                {
                    addArrayToDbus<uint64_t>(key, value, iface.get(),
                                             permission, systemConfiguration,
                                             path);
                }
                else
                {
                    addProperty(key, value.get<uint64_t>(), iface.get(),
                                systemConfiguration, path,
                                sdbusplus::asio::PropertyPermission::readOnly);
                }
                break;
            }
            case (nlohmann::json::value_t::number_float):
            {
                if (array)
                {
                    addArrayToDbus<double>(key, value, iface.get(), permission,
                                           systemConfiguration, path);
                }

                else
                {
                    addProperty(key, value.get<double>(), iface.get(),
                                systemConfiguration, path, permission);
                }
                break;
            }
            case (nlohmann::json::value_t::string):
            {
                if (array)
                {
                    addArrayToDbus<std::string>(key, value, iface.get(),
                                                permission, systemConfiguration,
                                                path);
                }
                else
                {
                    addProperty(key, value.get<std::string>(), iface.get(),
                                systemConfiguration, path, permission);
                }
                break;
            }
            default:
            {
                std::cerr << "Unexpected json type in system configuration "
                          << key << ": " << value.type_name() << "\n";
                break;
            }
        }
    }
    if (permission == sdbusplus::asio::PropertyPermission::readWrite)
    {
        createDeleteObjectMethod(jsonPointerPath, iface, objServer,
                                 systemConfiguration);
    }
    tryIfaceInitialize(iface);
}

void createAddObjectMethod(
    const std::string& jsonPointerPath, const std::string& path,
    nlohmann::json& systemConfiguration,
    sdbusplus::asio::object_server& objServer, const std::string& board)
{
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface = createInterface(
        objServer, path, "xyz.openbmc_project.AddObject", board);

    iface->register_method(
        "AddObject",
        [&systemConfiguration, &objServer,
         jsonPointerPath{std::string(jsonPointerPath)}, path{std::string(path)},
         board](const boost::container::flat_map<std::string, JsonVariantType>&
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

            std::ifstream schemaFile(
                std::string(configuration::schemaDirectory) + "/" +
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
            if (!configuration::validateJson(schema, newData))
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
            if (!configuration::writeJsonFiles(systemConfiguration))
            {
                std::cerr << "Error writing json files\n";
                throw DBusInternalError();
            }
            std::string dbusName = *name;

            std::regex_replace(dbusName.begin(), dbusName.begin(),
                               dbusName.end(), illegalDbusMemberRegex, "_");

            std::shared_ptr<sdbusplus::asio::dbus_interface> interface =
                createInterface(objServer, path + "/" + dbusName,
                                "xyz.openbmc_project.Configuration." + *type,
                                board, true);
            // permission is read-write, as since we just created it, must be
            // runtime modifiable
            populateInterfaceFromJson(
                systemConfiguration,
                jsonPointerPath + "/Exposes/" + std::to_string(lastIndex),
                interface, newData, objServer,
                sdbusplus::asio::PropertyPermission::readWrite);
        });
    tryIfaceInitialize(iface);
}

std::vector<std::weak_ptr<sdbusplus::asio::dbus_interface>>&
    getDeviceInterfaces(const nlohmann::json& device)
{
    return inventory[device["Name"].get<std::string>()];
}

} // namespace dbus_interface
