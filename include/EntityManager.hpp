/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#pragma once

#include "VariantVisitors.hpp"

#include <systemd/sd-journal.h>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <variant>

using BasicVariantType =
    std::variant<std::string, int64_t, uint64_t, double, int32_t, uint32_t,
                 int16_t, uint16_t, uint8_t, bool>;

constexpr const char* templateChar = "$";

inline void logDeviceAdded(const nlohmann::json& record)
{

    auto findType = record.find("Type");
    auto findAsset =
        record.find("xyz.openbmc_project.Inventory.Decorator.Asset");

    std::string model = "Unkown";
    std::string type = "Unknown";
    std::string sn = "Unkown";

    if (findType != record.end())
    {
        type = findType->get<std::string>();
    }
    if (findAsset != record.end())
    {
        auto findModel = findAsset->find("Model");
        auto findSn = findAsset->find("SerialNumber");
        if (findModel != findAsset->end())
        {
            model = findModel->get<std::string>();
        }
        if (findSn != findAsset->end())
        {
            const std::string* getSn = findSn->get_ptr<const std::string*>();
            if (getSn != nullptr)
            {
                sn = *getSn;
            }
            else
            {
                sn = findSn->dump();
            }
        }
    }

    sd_journal_send("MESSAGE=%s", "Inventory Added", "PRIORITY=%i", LOG_ERR,
                    "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryAdded",
                    "REDFISH_MESSAGE_ARGS=%s,%s,%s", model.c_str(),
                    type.c_str(), sn.c_str(), NULL);
}

inline void logDeviceRemoved(const nlohmann::json& record)
{
    auto findType = record.find("Type");
    auto findAsset =
        record.find("xyz.openbmc_project.Inventory.Decorator.Asset");

    std::string model = "Unkown";
    std::string type = "Unknown";
    std::string sn = "Unkown";

    if (findType != record.end())
    {
        type = findType->get<std::string>();
    }
    if (findAsset != record.end())
    {
        auto findModel = findAsset->find("Model");
        auto findSn = findAsset->find("SerialNumber");
        if (findModel != findAsset->end())
        {
            model = findModel->get<std::string>();
        }
        if (findSn != findAsset->end())
        {
            const std::string* getSn = findSn->get_ptr<const std::string*>();
            if (getSn != nullptr)
            {
                sn = *getSn;
            }
            else
            {
                sn = findSn->dump();
            }
        }
    }

    sd_journal_send("MESSAGE=%s", "Inventory Removed", "PRIORITY=%i", LOG_ERR,
                    "REDFISH_MESSAGE_ID=%s", "OpenBMC.0.1.InventoryRemoved",
                    "REDFISH_MESSAGE_ARGS=%s,%s,%s", model.c_str(),
                    type.c_str(), sn.c_str(), NULL);
}

enum class TemplateOperation
{
    addition,
    division,
    multiplication,
    subtraction,
    modulo,
};

// finds the template character (currently set to $) and replaces the value with
// the field found in a dbus object i.e. $ADDRESS would get populated with the
// ADDRESS field from a object on dbus
inline void templateCharReplace(
    nlohmann::json::iterator& keyPair,
    const boost::container::flat_map<std::string, BasicVariantType>&
        foundDevice,
    const size_t foundDeviceIdx)
{
    if (keyPair.value().type() == nlohmann::json::value_t::object ||
        keyPair.value().type() == nlohmann::json::value_t::array)
    {
        for (auto nextLayer = keyPair.value().begin();
             nextLayer != keyPair.value().end(); nextLayer++)
        {
            templateCharReplace(nextLayer, foundDevice, foundDeviceIdx);
        }
        return;
    }

    std::string* strPtr = keyPair.value().get_ptr<std::string*>();
    if (strPtr == nullptr)
    {
        return;
    }

    boost::replace_all(*strPtr, std::string(templateChar) + "index",
                       std::to_string(foundDeviceIdx));

    for (auto& foundDevicePair : foundDevice)
    {
        std::string templateName = templateChar + foundDevicePair.first;
        boost::iterator_range<std::string::const_iterator> find =
            boost::ifind_first(*strPtr, templateName);
        if (find)
        {
            size_t start = find.begin() - strPtr->begin();
            // check for additional operations
            if (!start && find.end() == strPtr->end())
            {
                std::visit([&](auto&& val) { keyPair.value() = val; },
                           foundDevicePair.second);
                return;
            }
            else if (find.end() == strPtr->end())
            {
                std::string val = std::visit(VariantToStringVisitor(),
                                             foundDevicePair.second);
                boost::replace_all(*strPtr,
                                   templateChar + foundDevicePair.first, val);
                return;
            }

            // save the prefix
            std::string prefix = strPtr->substr(0, start);

            // operate on the rest (+1 for trailing space)
            std::string end = strPtr->substr(start + templateName.size() + 1);

            std::vector<std::string> split;
            boost::split(split, end, boost::is_any_of(" "));

            // need at least 1 operation and number
            if (split.size() < 2)
            {
                std::cerr << "Syntax error on template replacement of "
                          << *strPtr << "\n";
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
            int number =
                std::visit(VariantToIntVisitor(), foundDevicePair.second);

            bool isOperator = true;
            TemplateOperation next = TemplateOperation::addition;

            auto it = split.begin();

            for (; it != split.end(); it++)
            {
                if (isOperator)
                {
                    if (*it == "+")
                    {
                        next = TemplateOperation::addition;
                    }
                    else if (*it == "-")
                    {
                        next = TemplateOperation::subtraction;
                    }
                    else if (*it == "*")
                    {
                        next = TemplateOperation::multiplication;
                    }
                    else if (*it == R"(%)")
                    {
                        next = TemplateOperation::modulo;
                    }
                    else if (*it == R"(/)")
                    {
                        next = TemplateOperation::division;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    int constant = 0;
                    try
                    {
                        constant = std::stoi(*it);
                    }
                    catch (std::invalid_argument&)
                    {
                        std::cerr << "Parameter not supported for templates "
                                  << *it << "\n";
                        continue;
                    }
                    switch (next)
                    {
                        case TemplateOperation::addition:
                        {
                            number += constant;
                            break;
                        }
                        case TemplateOperation::subtraction:
                        {
                            number -= constant;
                            break;
                        }
                        case TemplateOperation::multiplication:
                        {
                            number *= constant;
                            break;
                        }
                        case TemplateOperation::division:
                        {
                            number /= constant;
                            break;
                        }
                        case TemplateOperation::modulo:
                        {
                            number = number % constant;
                            break;
                        }

                        default:
                            break;
                    }
                }
                isOperator = !isOperator;
            }
            std::string result = prefix + std::to_string(number);

            if (it != split.end())
            {
                for (; it != split.end(); it++)
                {
                    result += " " + *it;
                }
            }
            keyPair.value() = result;

            // We probably just invalidated the pointer above, so set it to null
            strPtr = nullptr;
            break;
        }
    }

    strPtr = keyPair.value().get_ptr<std::string*>();
    if (strPtr == nullptr)
    {
        return;
    }

    // convert hex numbers to ints
    if (boost::starts_with(*strPtr, "0x"))
    {
        try
        {
            size_t pos = 0;
            int64_t temp = std::stoul(*strPtr, &pos, 0);
            if (pos == strPtr->size())
            {
                keyPair.value() = static_cast<uint64_t>(temp);
            }
        }
        catch (std::invalid_argument&)
        {
        }
        catch (std::out_of_range&)
        {
        }
    }
    // non-hex numbers
    else
    {
        try
        {
            uint64_t temp = boost::lexical_cast<uint64_t>(*strPtr);
            keyPair.value() = temp;
        }
        catch (boost::bad_lexical_cast&)
        {
        }
    }
}