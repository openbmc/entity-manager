// SPDX-License-Identifier: Apache-2.0
#include "entity_manager/config_validation.hpp"

#include <filesystem>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

TEST(ConfigValidation, ReusesCompiledSchemaForMultipleDocuments)
{
    config_validation::Validator validator(
        std::filesystem::path(EM_SOURCE_ROOT) / "schemas/global.json");
    nlohmann::json valid = {{"Exposes", nlohmann::json::array()},
                            {"Name", "Sample"},
                            {"Probe", "TRUE"},
                            {"Type", "Board"}};
    EXPECT_TRUE(validator.validate(valid));

    valid["Type"] = "NotAType";
    std::vector<config_validation::Error> errors;
    EXPECT_FALSE(validator.validate(valid, &errors));
    EXPECT_FALSE(errors.empty());
}

TEST(ConfigValidation, LoadsConfigurationsWithComments)
{
    auto document = config_validation::loadJson(
        std::filesystem::path(EM_SOURCE_ROOT) /
        "configurations/meta/clemente/clemente_hdd_nvme.json");
    EXPECT_TRUE(document.is_array());
}

TEST(ConfigValidation, SessionsCanValidateInParallel)
{
    config_validation::Validator validator(
        std::filesystem::path(EM_SOURCE_ROOT) / "schemas/global.json");
    const nlohmann::json valid = {{"Exposes", nlohmann::json::array()},
                                  {"Name", "Sample"},
                                  {"Probe", "TRUE"},
                                  {"Type", "Board"}};
    nlohmann::json invalid = valid;
    invalid["Type"] = "NotAType";

    bool validResult = false;
    bool invalidResult = true;
    {
        std::jthread first([&] {
            config_validation::Session session(validator);
            validResult = session.validate(valid);
        });
        std::jthread second([&] {
            config_validation::Session session(validator);
            invalidResult = session.validate(invalid);
        });
    }
    EXPECT_TRUE(validResult);
    EXPECT_FALSE(invalidResult);
}
