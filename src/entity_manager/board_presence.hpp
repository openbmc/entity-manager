#pragma once

#include <nlohmann/json.hpp>

#include <cstddef>
#include <flat_set>
#include <functional>
#include <string>
#include <vector>

namespace board_presence
{

using Names = std::flat_set<std::string, std::less<>>;

Names foundNames(const nlohmann::json& systemConfiguration);

bool boardFound(const nlohmann::json::object_t& object, const Names& found);

struct ArrayObject
{
    std::string property;
    size_t index{};

    bool operator==(const ArrayObject&) const = default;
};

struct RecordChange
{
    enum class Action
    {
        none,
        publish,
        withdraw,
        update,
    };

    Action action = Action::none;
    std::vector<ArrayObject> added;
    std::vector<ArrayObject> removed;
};

// What a change in the boards found means for a published platform record.
RecordChange recordChange(const nlohmann::json::object_t& record,
                          const Names& before, const Names& after);

} // namespace board_presence
