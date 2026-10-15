// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace config_validation
{

class Session;

struct Error
{
    std::string path;
    std::string message;
};

// Compiles a schema once for validation of multiple configuration documents.
// Relative references are resolved within the directory of the root schema.
class Validator
{
  public:
    explicit Validator(const std::filesystem::path& schemaPath);
    ~Validator();

    Validator(const Validator&) = delete;
    Validator& operator=(const Validator&) = delete;

    bool validate(const nlohmann::json& document,
                  std::vector<Error>* errors = nullptr) const;

  private:
    friend class Session;
    struct Impl;
    std::unique_ptr<Impl> impl;
};

// One session per worker thread. The compiled schema belongs to Validator;
// each session owns its own mutable regex cache and must not outlive it.
class Session
{
  public:
    explicit Session(const Validator& validator);
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    bool validate(const nlohmann::json& document,
                  std::vector<Error>* errors = nullptr);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

// Parse JSON documents with the same comment support as Entity Manager.
nlohmann::json loadJson(const std::filesystem::path& path);

} // namespace config_validation
