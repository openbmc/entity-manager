#include "board_presence.hpp"

#include <algorithm>

namespace board_presence
{

Names foundNames(const nlohmann::json& systemConfiguration)
{
    Names found;
    for (const auto& config : systemConfiguration)
    {
        auto name = config.find("Name");
        if (name != config.end() && name->is_string())
        {
            found.emplace(name->get<std::string>());
        }
    }
    return found;
}

bool boardFound(const nlohmann::json::object_t& object, const Names& found)
{
    auto board = object.find("Board");
    if (board == object.end() || !board->second.is_string())
    {
        return true;
    }
    return found.contains(board->second.get_ref<const std::string&>());
}

RecordChange recordChange(const nlohmann::json::object_t& record,
                          const Names& before, const Names& after)
{
    RecordChange change;
    const bool wasPublished = boardFound(record, before);
    const bool isPublished = boardFound(record, after);
    if (wasPublished != isPublished)
    {
        change.action = isPublished ? RecordChange::Action::publish
                                    : RecordChange::Action::withdraw;
        return change;
    }
    if (!isPublished)
    {
        return change;
    }

    for (const auto& [property, value] : record)
    {
        // Only an array of objects is published as an interface per object.
        const auto* array = value.get_ptr<const nlohmann::json::array_t*>();
        if (array == nullptr || array->empty() ||
            !std::ranges::all_of(*array, &nlohmann::json::is_object))
        {
            continue;
        }
        for (size_t index = 0; index < array->size(); ++index)
        {
            const auto* object =
                (*array)[index].get_ptr<const nlohmann::json::object_t*>();
            if (object == nullptr)
            {
                continue;
            }
            const bool wasThere = boardFound(*object, before);
            const bool isThere = boardFound(*object, after);
            if (wasThere != isThere)
            {
                (isThere ? change.added : change.removed)
                    .push_back({property, index});
            }
        }
    }
    if (!change.added.empty() || !change.removed.empty())
    {
        change.action = RecordChange::Action::update;
    }
    return change;
}

} // namespace board_presence
