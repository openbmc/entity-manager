#include "utils.hpp"

#include "../variant_visitors.hpp"
#include "expression.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <sdbusplus/bus/match.hpp>

#include <fstream>
#include <iostream>

namespace em_utils
{

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static bool powerStatusOn = false;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::unique_ptr<sdbusplus::bus::match_t> powerMatch = nullptr;

constexpr const char* templateChar = "$";

bool isPowerOn()
{
    if (!powerMatch)
    {
        throw std::runtime_error("Power Match Not Created");
    }
    return powerStatusOn;
}

void setupPowerMatch(const std::shared_ptr<sdbusplus::asio::connection>& conn)
{
    powerMatch = std::make_unique<sdbusplus::bus::match_t>(
        static_cast<sdbusplus::bus_t&>(*conn),
        "type='signal',interface='" + std::string(properties::interface) +
            "',path='" + std::string(power::path) + "',arg0='" +
            std::string(power::interface) + "'",
        [](sdbusplus::message_t& message) {
            std::string objectName;
            boost::container::flat_map<std::string, std::variant<std::string>>
                values;
            message.read(objectName, values);
            auto findState = values.find(power::property);
            if (findState != values.end())
            {
                powerStatusOn = boost::ends_with(
                    std::get<std::string>(findState->second), "Running");
            }
        });

    conn->async_method_call(
        [](boost::system::error_code ec,
           const std::variant<std::string>& state) {
            if (ec)
            {
                return;
            }
            powerStatusOn =
                boost::ends_with(std::get<std::string>(state), "Running");
        },
        power::busname, power::path, properties::interface, properties::get,
        power::interface, power::property);
}

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

    boost::replace_all(*strPtr, std::string(templateChar) + "index",
                       std::to_string(index));
    if (replaceStr)
    {
        boost::replace_all(*strPtr, *replaceStr, std::to_string(index));
    }

    for (const auto& [propName, propValue] : interface)
    {
        std::string templateName = templateChar + propName;
        boost::iterator_range<std::string::const_iterator> find =
            boost::ifind_first(*strPtr, templateName);
        if (!find)
        {
            continue;
        }

        size_t start = find.begin() - strPtr->begin();

        // check for additional operations
        if ((start == 0U) && find.end() == strPtr->end())
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
            boost::ireplace_all(*strPtr, templateName, val);
            continue;
        }

        // save the prefix
        std::string prefix = strPtr->substr(0, start);

        // operate on the rest
        std::string end = strPtr->substr(nextItemIdx);

        std::vector<std::string> split;
        boost::split(split, end, boost::is_any_of(" "));

        // need at least 1 operation and number
        if (split.size() < 2)
        {
            std::cerr << "Syntax error on template replacement of " << *strPtr
                      << "\n";
            for (const std::string& data : split)
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
        auto exprBegin = split.begin();
        auto exprEnd = split.end();

        number = expression::evaluate(number, exprBegin, exprEnd);

        std::string replaced(find.begin(), find.end());
        while (exprBegin != exprEnd)
        {
            replaced.append(" ").append(*exprBegin++);
        }
        ret = replaced;

        std::string result = prefix + std::to_string(number);
        while (exprEnd != split.end())
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
    if (boost::starts_with(strView, "0x"))
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

} // namespace em_utils
