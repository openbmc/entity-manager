/*
// Copyright (c) 2017 Intel Corporation
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

#include "filesystem.hpp"

#include <Utils.hpp>
#include <fstream>
#include <regex>
#include <sdbusplus/bus/match.hpp>
#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>

namespace fs = std::filesystem;
static bool powerStatusOn = false;
static std::unique_ptr<sdbusplus::bus::match::match> powerMatch = nullptr;

bool findFiles(const fs::path& dirPath, const std::string& matchString,
               std::vector<fs::path>& foundPaths)
{
    if (!fs::exists(dirPath))
        return false;

    std::regex search(matchString);
    std::smatch match;
    for (const auto& p : fs::directory_iterator(dirPath))
    {
        std::string path = p.path().string();
        if (std::regex_search(path, match, search))
        {
            foundPaths.emplace_back(p.path());
        }
    }
    return true;
}

bool validateJson(const nlohmann::json& schemaFile, const nlohmann::json& input)
{
    valijson::Schema schema;
    valijson::SchemaParser parser;
    valijson::adapters::NlohmannJsonAdapter schemaAdapter(schemaFile);
    parser.populateSchema(schemaAdapter, schema);
    valijson::Validator validator;
    valijson::adapters::NlohmannJsonAdapter targetAdapter(input);
    if (!validator.validate(schema, targetAdapter, NULL))
    {
        return false;
    }
    return true;
}

bool isPowerOn(void)
{
    if (!powerMatch)
    {
        throw std::runtime_error("Power Match Not Created");
    }
    return powerStatusOn;
}

void setupPowerMatch(const std::shared_ptr<sdbusplus::asio::connection>& conn)
{
    // create a match for powergood changes, first time do a method call to
    // cache the correct value
    std::function<void(sdbusplus::message::message & message)> eventHandler =
        [](sdbusplus::message::message& message) {
            std::string objectName;
            boost::container::flat_map<std::string, std::variant<int32_t, bool>>
                values;
            message.read(objectName, values);
            auto findPgood = values.find("pgood");
            if (findPgood != values.end())
            {
                powerStatusOn = std::get<int32_t>(findPgood->second);
            }
        };

    powerMatch = std::make_unique<sdbusplus::bus::match::match>(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',interface='org.freedesktop.DBus.Properties',path_"
        "namespace='/xyz/openbmc_project/Chassis/Control/"
        "Power0',arg0='xyz.openbmc_project.Chassis.Control.Power'",
        eventHandler);
}