
#include "em_config.hpp"

#include <nlohmann/json.hpp>

template <typename T>
const T* getConfigProp(const nlohmann::json::object_t& config,
                       const std::string& name)
{
    auto nameIt = config.find(name);

    if (nameIt == config.end())
    {
        throw std::invalid_argument(
            std::format("property '{}' not found", name));
    }

    const std::string* ptr = nameIt->second.get_ptr<const T*>();

    if (ptr == nullptr)
    {
        throw std::invalid_argument(
            std::format("property '{}' has wrong type", name));
    }

    return ptr;
}

EMConfig EMConfig::fromJson(const nlohmann::json& config)
{
    const nlohmann::json::object_t* obj =
        config.get_ptr<const nlohmann::json::object_t*>();

    if (obj == nullptr)
    {
        throw std::invalid_argument("config was not an object");
    }

    return EMConfig(*obj);
}

static void parseProbe(std::vector<std::string>& probeStmt,
                       const nlohmann::json::object_t& config)
{
    auto findProbe = config.find("Probe");

    if (findProbe == config.end())
    {
        throw std::invalid_argument("No 'Probe' statement");
    }

    if ((*findProbe).second.type() == nlohmann::json::value_t::array)
    {
        const auto* probeArray =
            findProbe->second.get_ptr<const nlohmann::json::array_t*>();

        for (auto p : *probeArray)
        {
            probeStmt.push_back(*(p.get_ptr<const std::string*>()));
        }
    }
    else
    {
        probeStmt.push_back(*(findProbe->second.get_ptr<const std::string*>()));
    }
}

static bool parseLogging(const nlohmann::json& json)
{
    auto logging = json.find("Logging");
    if (logging != json.end())
    {
        const auto* ptr = logging->get_ptr<const std::string*>();
        if (ptr != nullptr)
        {
            if (*ptr == "Off")
            {
                return false;
            }
        }
    }
    return true;
}

static void parseExposes(std::vector<nlohmann::json::object_t>& exposesRecords,
                         const nlohmann::json::object_t& config)
{
    auto findExposes = config.find("Exposes");

    if (findExposes == config.end())
    {
        throw std::invalid_argument("No 'Exposes' array");
    }

    const auto* exposesArray =
        findExposes->second.get_ptr<const nlohmann::json::array_t*>();

    if (exposesArray == nullptr)
    {
        throw std::invalid_argument("'Exposes' array has wrong type");
    }

    for (const nlohmann::json& item : *exposesArray)
    {
        const nlohmann::json::object_t* obj =
            item.get_ptr<const nlohmann::json::object_t*>();
        if (obj == nullptr)
        {
            throw std::invalid_argument("record was not an object");
        }

        exposesRecords.emplace_back(*obj);
    }
}

EMConfig::EMConfig(const nlohmann::json::object_t& config) :
    name(*getConfigProp<std::string>(config, "Name")),
    type(*getConfigProp<std::string>(config, "Type")),
    deviceHasLogging(parseLogging(config))
{
    parseProbe(probeStmt, config);
    parseExposes(exposesRecords, config);

    // parse extra interfaces
    for (const auto& [key, value] : config)
    {
        if (key.starts_with("xyz.openbmc_project."))
        {
            const nlohmann::json::object_t* obj =
                value.get_ptr<const nlohmann::json::object_t*>();
            if (obj == nullptr)
            {
                throw std::invalid_argument("record was not an object");
            }
            extraInterfaces.insert_or_assign(key, *obj);
        }
    }
}

nlohmann::json EMConfig::toJson()
{
    nlohmann::json res;

    res["Name"] = name;
    res["Type"] = type;

    if (!deviceHasLogging)
    {
        res["Logging"] = "Off";
    }

    res["Probe"] = nlohmann::json::array_t();
    for (const auto& probe : probeStmt)
    {
        res["Probe"].push_back(probe);
    }

    res["Exposes"] = nlohmann::json::array_t();
    for (const auto& record : exposesRecords)
    {
        res["Exposes"].push_back(record);
    }

    for (auto [k, v] : extraInterfaces)
    {
        res[k] = v;
    }

    return res;
}
