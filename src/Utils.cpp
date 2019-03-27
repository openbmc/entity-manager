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
#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>

namespace fs = std::filesystem;

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

bool getI2cDevicePaths(const fs::path& dirPath,
                      boost::container::flat_map<size_t, fs::path>& busPaths)
{
    if (!fs::exists(dirPath))
    {
        return false;
    }

    // Regex for matching the path
    std::regex searchPath(std::string(R"(i2c-\d+$)"));
    // Regex for matching the bus numbers
    std::regex searchBus(std::string(R"(\w[^-]*$)"));
    std::smatch matchPath;
    std::smatch matchBus;
    for (const auto& p : fs::directory_iterator(dirPath))
    {
        std::string path = p.path().string();
        if (std::regex_search(path, matchPath, searchPath))
        {
            if (std::regex_search(path, matchBus, searchBus));
            {
                size_t bus = stoul(*matchBus.begin());
                busPaths.insert(std::pair<size_t, fs::path>(bus, p.path()));
            }
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
