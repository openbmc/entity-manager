#include "config_pointer.hpp"

#include <phosphor-logging/lg2.hpp>

#include <string>

ConfigPointer::ConfigPointer(const std::string& boardIdIn) : boardId(boardIdIn)
{}

ConfigPointer::ConfigPointer(const std::string& boardIdIn,
                             uint64_t exposesIndexIn) :
    boardId(boardIdIn), exposesIndex(exposesIndexIn)
{}
ConfigPointer::ConfigPointer(const std::string& boardIdIn,
                             uint64_t exposesIndexIn,
                             const std::string& propNameIn) :
    boardId(boardIdIn), exposesIndex(exposesIndexIn), propertyName(propNameIn)
{}

// This is for properties directly on a board config and not part of an
// exposes record
ConfigPointer::ConfigPointer(const std::string& boardIdIn,
                             const std::string& propNameIn) :
    boardId(boardIdIn), propertyName(propNameIn)
{}

// This represents indexing into a board, exposes record, config record, and
// array element within
ConfigPointer::ConfigPointer(
    const std::string& boardIdIn, uint64_t exposesIndexIn,
    const std::string& propNameIn, uint64_t arrayIndexIn) :
    boardId(boardIdIn), exposesIndex(exposesIndexIn), propertyName(propNameIn),
    arrayIndex(arrayIndexIn)
{}

ConfigPointer ConfigPointer::withExposesIndexAndName(
    const uint64_t index, const std::string& name) const
{
    return {boardId, index, name};
}

ConfigPointer ConfigPointer::withArrayIndex(uint64_t arrayIndex) const
{
    if (!exposesIndex.has_value() || !propertyName.has_value())
    {
        lg2::error("error constructing ConfigPointer");
        return ConfigPointer(boardId);
    }
    return {boardId, exposesIndex.value(), propertyName.value(), arrayIndex};
}

ConfigPointer ConfigPointer::withName(const std::string& name) const
{
    ConfigPointer result = *this;
    if (propertyName.has_value() || arrayIndex.has_value())
    {
        result.memberName = name;
    }
    else
    {
        result.propertyName = name;
    }
    return result;
}

nlohmann::json* ConfigPointer::findRecord(nlohmann::json& board) const
{
    nlohmann::json* target = &board;
    if (exposesIndex)
    {
        auto it = target->find("Exposes");
        if (it == target->end() || !it->is_array() ||
            it->size() <= *exposesIndex)
        {
            lg2::error("error: config ptr: invalid exposes index {INDEX}",
                       "INDEX", *exposesIndex);
            return nullptr;
        }
        target = &(*it)[*exposesIndex];
    }
    if (propertyName)
    {
        auto it = target->find(*propertyName);
        if (it == target->end())
        {
            lg2::error("error: config ptr: property {NAME} not found", "NAME",
                       *propertyName);
            return nullptr;
        }
        target = &(*it);
    }
    return target;
}

nlohmann::json* ConfigPointer::findValue(nlohmann::json& record) const
{
    nlohmann::json* target = &record;
    if (arrayIndex)
    {
        if (!target->is_array() || target->size() <= *arrayIndex)
        {
            lg2::error("error: config ptr: invalid array index {INDEX}",
                       "INDEX", *arrayIndex);
            return nullptr;
        }
        target = &(*target)[*arrayIndex];
    }
    if (memberName)
    {
        auto it = target->find(*memberName);
        if (it == target->end())
        {
            lg2::error("error: config ptr: property {NAME} not found", "NAME",
                       *memberName);
            return nullptr;
        }
        target = &(*it);
    }
    return target;
}

bool ConfigPointer::write(const nlohmann::json& value,
                          nlohmann::json& systemConfiguration) const
{
    lg2::debug("config ptr: writing value: {VALUE}", "VALUE", value);
    auto targetIt = systemConfiguration.find(boardId);
    if (targetIt == systemConfiguration.end())
    {
        lg2::error("error: config ptr: board id {ID} not found", "ID", boardId);
        return false;
    }
    nlohmann::json* record = findRecord(*targetIt);
    if (record == nullptr)
    {
        return false;
    }
    nlohmann::json* target = findValue(*record);
    if (target == nullptr)
    {
        return false;
    }
    if (exposesIndex && !propertyName && value.is_null())
    {
        // Keep the slot so pointers to later exposes stay valid.
        (*target)["Status"] = "disabled";
    }
    else
    {
        *target = value;
    }
    return true;
}

static bool replaceObject(const nlohmann::json& value,
                          nlohmann::json::object_t& object)
{
    const auto* replacement = value.get_ptr<const nlohmann::json::object_t*>();
    if (replacement == nullptr)
    {
        return false;
    }
    object = *replacement;
    return true;
}

bool ConfigPointer::writeExpose(const nlohmann::json& value,
                                EMConfig& board) const
{
    if (board.exposesRecords.size() <= *exposesIndex)
    {
        lg2::error("error: config ptr: invalid exposes index {INDEX}", "INDEX",
                   *exposesIndex);
        return false;
    }
    nlohmann::json::object_t& record = board.exposesRecords[*exposesIndex];
    if (!propertyName)
    {
        if (value.is_null())
        {
            // Keep the slot so pointers to later exposes stay valid.
            record["Status"] = "disabled";
            return true;
        }
        return replaceObject(value, record);
    }
    auto it = record.find(*propertyName);
    if (it == record.end())
    {
        lg2::error("error: config ptr: property {NAME} not found", "NAME",
                   *propertyName);
        return false;
    }
    nlohmann::json* target = findValue(it->second);
    if (target == nullptr)
    {
        return false;
    }
    *target = value;
    return true;
}

bool ConfigPointer::writeBoardProperty(const nlohmann::json& value,
                                       EMConfig& board) const
{
    if (!memberName && !arrayIndex &&
        (*propertyName == "Name" || *propertyName == "Type"))
    {
        const auto* str = value.get_ptr<const std::string*>();
        if (str == nullptr)
        {
            return false;
        }
        (*propertyName == "Name" ? board.name : board.type) = *str;
        return true;
    }
    auto it = board.extraInterfaces.find(*propertyName);
    if (it == board.extraInterfaces.end())
    {
        return false;
    }
    if (!memberName && !arrayIndex)
    {
        if (value.is_null())
        {
            board.extraInterfaces.erase(it);
            return true;
        }
        return replaceObject(value, it->second);
    }
    if (!memberName)
    {
        return false;
    }
    auto member = it->second.find(*memberName);
    if (member == it->second.end())
    {
        return false;
    }
    nlohmann::json* target = &member->second;
    if (arrayIndex)
    {
        if (!target->is_array() || target->size() <= *arrayIndex)
        {
            return false;
        }
        target = &(*target)[*arrayIndex];
    }
    *target = value;
    return true;
}

bool ConfigPointer::write(const nlohmann::json& value,
                          SystemConfiguration& systemConfiguration) const
{
    lg2::debug("config ptr: writing value: {VALUE}", "VALUE", value);
    auto targetIt = systemConfiguration.find(boardId);
    if (targetIt == systemConfiguration.end())
    {
        lg2::error("error: config ptr: board id {ID} not found", "ID", boardId);
        return false;
    }
    EMConfig& board = targetIt->second;
    // The source may refer to a property in the configuration being replaced.
    nlohmann::json newValue = value;
    if (exposesIndex)
    {
        return writeExpose(newValue, board);
    }
    if (propertyName)
    {
        return writeBoardProperty(newValue, board);
    }
    auto parsed = EMConfig::fromJson(newValue);
    if (!parsed)
    {
        return false;
    }
    board = std::move(*parsed);
    return true;
}
