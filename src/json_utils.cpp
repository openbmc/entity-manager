#include "json_utils.hpp"

#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <string>
#include <string_view>

const std::string* getStringFromObject(const nlohmann::json::object_t& object,
                                       std::string_view key)
{
    auto it = object.find(key);
    if (it == object.end())
    {
        lg2::error("Unable to find {KEY}", "KEY", key);
        return nullptr;
    }
    return it->second.get_ptr<const std::string*>();
}

std::string* getStringFromObject(nlohmann::json::object_t& object,
                                 std::string_view key)
{
    auto it = object.find(key);
    if (it == object.end())
    {
        lg2::error("Unable to find {KEY}", "KEY", key);
        return nullptr;
    }
    return it->second.get_ptr<std::string*>();
}
