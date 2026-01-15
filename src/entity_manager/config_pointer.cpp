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
    return ConfigPointer(boardId, index, name);
}

ConfigPointer ConfigPointer::withArrayIndex(uint64_t arrayIndex) const
{
    if (!exposesIndex.has_value() || !propertyName.has_value())
    {
        lg2::error("error constructing ConfigPointer");
        return ConfigPointer(boardId);
    }
    return ConfigPointer(boardId, exposesIndex.value(), propertyName.value(),
                         arrayIndex);
}
