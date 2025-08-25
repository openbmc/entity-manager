#include "utils.hpp"

#include "../utils.hpp"
#include "../variant_visitors.hpp"
#include "expression.hpp"

#include <boost/algorithm/string/case_conv.hpp>
#include <sdbusplus/bus/match.hpp>

#include <fstream>
#include <iostream>
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
        std::cerr << "Can't read " << versionFile << "\n";
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

    std::filesystem::create_directory(configurationOutDir);
    std::ifstream hashFile(versionHashFile);
    if (hashFile.good())
    {
        std::string hashString;
        hashFile >> hashString;

        if (expectedHash == hashString)
        {
            return true;
        }
        hashFile.close();
    }

    std::ofstream output(versionHashFile);
    output << expectedHash;
    return false;
}

// Replaces the template character like the other version of this function,
// but checks all properties on all interfaces provided to do the substitution
// with.
std::optional<std::string> templateCharReplace(
    nlohmann::json::iterator& keyPair, const DBusObject& object,
    const size_t index, const std::optional<std::string>& replaceStr)
{
    for (const auto& [_, interface] : object)
    {
        auto ret = templateCharReplace(keyPair, interface, index, replaceStr);
        if (ret)
        {
            return ret;
        }
    }
    return std::nullopt;
}

// finds the template character (currently set to $) and replaces the value with
// the field found in a dbus object i.e. $ADDRESS would get populated with the
// ADDRESS field from a object on dbus
std::optional<std::string> templateCharReplace(
    nlohmann::json::iterator& keyPair, const DBusInterface& interface,
    const size_t index, const std::optional<std::string>& replaceStr)
{
    std::optional<std::string> ret = std::nullopt;

    if (keyPair.value().type() == nlohmann::json::value_t::object ||
        keyPair.value().type() == nlohmann::json::value_t::array)
    {
        for (auto nextLayer = keyPair.value().begin();
             nextLayer != keyPair.value().end(); nextLayer++)
        {
            templateCharReplace(nextLayer, interface, index, replaceStr);
        }
        return ret;
    }

    std::string* strPtr = keyPair.value().get_ptr<std::string*>();
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
        auto [start, endIdx] = ifindFirst(*strPtr, templateName);
        if (start == std::string::npos)
        {
            continue;
        }

        // check for additional operations
        if ((start == 0U) && endIdx == strPtr->size())
        {
            std::visit([&](auto&& val) { keyPair.value() = val; }, propValue);
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
            ireplaceAll(*strPtr, templateName, val);
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
            std::cerr << "Syntax error on template replacement of " << *strPtr
                      << "\n";
            for (const std::string& data : splitResult)
            {
                std::cerr << data << " ";
            }
            std::cerr << "\n";
            continue;
        }

        // we assume that the replacement is a number, because we can
        // only do math on numbers.. we might concatenate strings in the
        // future, but thats later
        int number = std::visit(VariantToIntVisitor(), propValue);
        auto exprBegin = splitResult.begin();
        auto exprEnd = splitResult.end();

        number = expression::evaluate(number, exprBegin, exprEnd);

        std::string replaced(strPtr->begin() + start, strPtr->begin() + endIdx);
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
        keyPair.value() = result;

        // We probably just invalidated the pointer abovei,
        // reset and continue to handle multiple templates
        strPtr = keyPair.value().get_ptr<std::string*>();
        if (strPtr == nullptr)
        {
            break;
        }
    }

    strPtr = keyPair.value().get_ptr<std::string*>();
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
    const char* strDataEndPtr = strView.data() + strView.size();
    const std::from_chars_result res =
        std::from_chars(strView.data(), strDataEndPtr, temp, base);
    if (res.ec == std::errc{} && res.ptr == strDataEndPtr)
    {
        keyPair.value() = temp;
    }

    return ret;
}

std::string buildInventorySystemPath(std::string& boardName,
                                     const std::string& boardType)
{
    std::string path = "/xyz/openbmc_project/inventory/system/";
    std::string boardTypeLower = boost::algorithm::to_lower_copy(boardType);

    std::regex_replace(boardName.begin(), boardName.begin(), boardName.end(),
                       illegalDbusMemberRegex, "_");

    return std::format("{}{}/{}", path, boardTypeLower, boardName);
}
} // namespace em_utils
