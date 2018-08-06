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

#include <Utils.hpp>
#include <experimental/filesystem>
#include <fstream>
#include <regex>
#include <valijson/adapters/nlohmann_json_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validator.hpp>

namespace fs = std::experimental::filesystem;

bool find_files(const fs::path &dir_path, const std::string &match_string,
                std::vector<fs::path> &found_paths, unsigned int symlink_depth)
{
    if (!fs::exists(dir_path))
        return false;

    fs::directory_iterator end_itr;
    std::regex search(match_string);
    std::smatch match;
    for (auto &p : fs::recursive_directory_iterator(dir_path))
    {
        std::string path = p.path().string();
        if (std::regex_search(path, match, search))
        {
            found_paths.emplace_back(p.path());
        }
        // since we're using a recursve iterator, these should only be symlink
        // dirs
        else if (is_directory(p) && symlink_depth)
        {
            find_files(p.path(), match_string, found_paths, symlink_depth - 1);
        }
    }
    return true;
}

bool validateJson(const nlohmann::json &schemaFile, const nlohmann::json &input)
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