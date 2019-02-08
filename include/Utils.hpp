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

#pragma once
#include "filesystem.hpp"

#include <nlohmann/json.hpp>
#include <sdbusplus/exception.hpp>

bool findFiles(const std::filesystem::path& dirPath,
               const std::string& matchString,
               std::vector<std::filesystem::path>& foundPaths);

bool validateJson(const nlohmann::json& schemaFile,
                  const nlohmann::json& input);
struct DBusInternalError final : public sdbusplus::exception_t
{
    const char* name() const noexcept override
    {
        return "org.freedesktop.DBus.Error.Failed";
    };
    const char* description() const noexcept override
    {
        return "internal error";
    };
    const char* what() const noexcept override
    {
        return "org.freedesktop.DBus.Error.Failed: "
               "internal error";
    };
};
