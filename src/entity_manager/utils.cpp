#include "utils.hpp"

#include "../utils.hpp"
#include "../variant_visitors.hpp"
#include "expression.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/bus/match.hpp>

#include <fstream>
#include <regex>

const std::regex illegalDbusMemberRegex("[^A-Za-z0-9_]");

namespace em_utils
{

constexpr const char* templateChar = "$";

bool fwVersionIsSame()
{
    std::ifstream version(versionFile);
    if (!version.good())
    {
        lg2::error("Can't read {PATH}", "PATH", versionFile);
        return false;
    }

    std::string versionData;
    std::string line;
    while (std::getline(version, line))
    {
        versionData += line;
    }

    std::string expectedHash =
        std::to_string(std::hash<std::string>{}(versionData));

    std::error_code ec;
    std::filesystem::create_directory(configurationOutDir, ec);

    if (ec)
    {
        lg2::error("could not create directory {DIR}", "DIR",
                   configurationOutDir);
        return false;
    }

    std::ifstream hashFile(versionHashFile);
    if (hashFile.good())
    {
        std::string hashString;
        hashFile >> hashString;

        if (expectedHash == hashString)
        {
            lg2::debug(
                "The firmware version is unchanged since the last boot, hash value of versionFile is: {HASH}",
                "HASH", hashString);
            return true;
        }
        lg2::debug(
            "The firmware version has changed since the last boot, hash value of current versionFile is: {EXPECTED_HASH}, hash value of versionFile of last boot is: {LAST_HASH}",
            "EXPECTED_HASH", expectedHash, "LAST_HASH", hashString);
        hashFile.close();
    }

    std::ofstream output(versionHashFile);
    output << expectedHash;
    return false;
}

void handleLeftOverTemplateVars(nlohmann::json& value)
{
    nlohmann::json::object_t* objPtr =
        value.get_ptr<nlohmann::json::object_t*>();
    if (objPtr != nullptr)
    {
        for (auto& nextLayer : *objPtr)
        {
            handleLeftOverTemplateVars(nextLayer.second);
        }
        return;
    }

    nlohmann::json::array_t* arrPtr = value.get_ptr<nlohmann::json::array_t*>();
    if (arrPtr != nullptr)
    {
        for (auto& nextLayer : *arrPtr)
        {
            handleLeftOverTemplateVars(nextLayer);
        }
        return;
    }

    std::string* strPtr = value.get_ptr<std::string*>();
    if (strPtr == nullptr)
    {
        return;
    }

    // Walking through the string to find $<templateVar>
    while (true)
    {
        std::ranges::subrange<std::string::const_iterator> findStart =
            iFindFirst(*strPtr, std::string_view(templateChar));

        if (!findStart)
        {
            break;
        }

        std::ranges::subrange<std::string::iterator> searchRange(
            strPtr->begin() + (findStart.end() - strPtr->begin()),
            strPtr->end());
        std::ranges::subrange<std::string::const_iterator> findSpace =
            iFindFirst(searchRange, " ");

        std::string::const_iterator templateVarEnd;

        if (!findSpace)
        {
            // No space means the template var spans to the end of
            // of the keyPair value
            templateVarEnd = strPtr->end();
        }
        else
        {
            // A space marks the end of a template var
            templateVarEnd = findSpace.begin();
        }

        lg2::error(
            "There's still template variable {VAR} un-replaced. Removing it from the string.\n",
            "VAR", std::string(findStart.begin(), templateVarEnd));
        strPtr->erase(findStart.begin(), templateVarEnd);
    }
}

// Replaces the template character like the other version of this function,
// but checks all properties on all interfaces provided to do the substitution
// with.
std::optional<std::string> templateCharReplace(
    nlohmann::json& value, const DBusObject& object, const size_t index,
    const std::optional<std::string>& replaceStr, bool handleLeftOver)
{
    for (const auto& [_, interface] : object)
    {
        auto ret = templateCharReplace(value, interface, index, replaceStr);
        if (ret)
        {
            if (handleLeftOver)
            {
                handleLeftOverTemplateVars(value);
            }
            return ret;
        }
    }
    if (handleLeftOver)
    {
        handleLeftOverTemplateVars(value);
    }
    return std::nullopt;
}

// finds the template character (currently set to $) and replaces the value with
// the field found in a dbus object i.e. $ADDRESS would get populated with the
// ADDRESS field from a object on dbus
std::optional<std::string> templateCharReplace(
    nlohmann::json& value, const DBusInterface& interface, const size_t index,
    const std::optional<std::string>& replaceStr)
{
    std::optional<std::string> ret = std::nullopt;

    nlohmann::json::object_t* objPtr =
        value.get_ptr<nlohmann::json::object_t*>();
    if (objPtr != nullptr)
    {
        for (auto& [key, value] : *objPtr)
        {
            templateCharReplace(value, interface, index, replaceStr);
        }
        return ret;
    }

    nlohmann::json::array_t* arrPtr = value.get_ptr<nlohmann::json::array_t*>();
    if (arrPtr != nullptr)
    {
        for (auto& value : *arrPtr)
        {
            templateCharReplace(value, interface, index, replaceStr);
        }
        return ret;
    }

    std::string* strPtr = value.get_ptr<std::string*>();
    if (strPtr == nullptr)
    {
        return ret;
    }

    replaceAll(*strPtr, std::string(templateChar) + "index",
               std::to_string(index));
    if (replaceStr)
    {
        replaceAll(*strPtr, *replaceStr, std::to_string(index));
    }

    for (const auto& [propName, propValue] : interface)
    {
        std::string templateName = templateChar + propName;
        std::ranges::subrange<std::string::const_iterator> find =
            iFindFirst(*strPtr, templateName);
        if (!find)
        {
            continue;
        }

        size_t start = find.begin() - strPtr->begin();

        // check for additional operations
        if ((start == 0U) && find.end() == strPtr->end())
        {
            std::visit([&](auto&& val) { value = val; }, propValue);
            return ret;
        }

        constexpr const std::array<char, 5> mathChars = {'+', '-', '%', '*',
                                                         '/'};
        size_t nextItemIdx = start + templateName.size() + 1;

        if (nextItemIdx > strPtr->size() ||
            std::find(mathChars.begin(), mathChars.end(),
                      strPtr->at(nextItemIdx)) == mathChars.end())
        {
            std::string val = std::visit(VariantToStringVisitor(), propValue);
            iReplaceAll(*strPtr, templateName, val);
            continue;
        }

        // save the prefix
        std::string prefix = strPtr->substr(0, start);

        // operate on the rest
        std::string end = strPtr->substr(nextItemIdx);

        std::vector<std::string> splitResult = split(end, ' ');

        // need at least 1 operation and number
        if (splitResult.size() < 2)
        {
            lg2::error("Syntax error on template replacement of {STR}", "STR",
                       *strPtr);
            for (const std::string& data : splitResult)
            {
                lg2::error("{SPLIT} ", "SPLIT", data);
            }
            lg2::error("");
            continue;
        }

        // we assume that the replacement is a number, because we can
        // only do math on numbers.. we might concatenate strings in the
        // future, but thats later
        int number = std::visit(VariantToIntVisitor(), propValue);
        auto exprBegin = splitResult.begin();
        auto exprEnd = splitResult.end();

        number = expression::evaluate(number, exprBegin, exprEnd);

        std::string replaced(find.begin(), find.end());
        while (exprBegin != exprEnd)
        {
            replaced.append(" ").append(*exprBegin++);
        }
        ret = replaced;

        std::string result = prefix + std::to_string(number);
        while (exprEnd != splitResult.end())
        {
            result.append(" ").append(*exprEnd++);
        }
        value = result;

        // We probably just invalidated the pointer abovei,
        // reset and continue to handle multiple templates
        strPtr = value.get_ptr<std::string*>();
        if (strPtr == nullptr)
        {
            break;
        }
    }

    strPtr = value.get_ptr<std::string*>();
    if (strPtr == nullptr)
    {
        return ret;
    }

    std::string_view strView = *strPtr;
    int base = 10;
    if (strView.starts_with("0x"))
    {
        strView.remove_prefix(2);
        base = 16;
    }

    uint64_t temp = 0;
    bool fullMatch = false;
    const std::from_chars_result res =
        fromCharsWrapper(strView, temp, fullMatch, base);
    if (res.ec == std::errc{} && fullMatch)
    {
        value = temp;
    }

    return ret;
}

std::string buildInventorySystemPath(std::string& boardName,
                                     const std::string& boardType)
{
    std::string path = "/xyz/openbmc_project/inventory/system/";
    std::string boardTypeLower = toLowerCopy(boardType);
    std::regex_replace(boardName.begin(), boardName.begin(), boardName.end(),
                       illegalDbusMemberRegex, "_");

    return std::format("{}{}/{}", path, boardTypeLower, boardName);
}
} // namespace em_utils
