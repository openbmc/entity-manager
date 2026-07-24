// SPDX-License-Identifier: Apache-2.0
#include "entity_manager/schema_defaults.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>

#include <gtest/gtest.h>

namespace
{
using Json = nlohmann::json;

class SchemaDefaultsTest : public testing::Test
{
  protected:
    std::filesystem::path directory;

    void SetUp() override
    {
        auto pattern = (std::filesystem::temp_directory_path() /
                        "entity-manager-defaults-XXXXXX")
                           .string();
        const char* created = mkdtemp(pattern.data());
        ASSERT_NE(created, nullptr);
        directory = created;
        write("exposes_record.json", R"({"$defs": {"ConfigSchema": {"oneOf": [
            {"$ref": "device.json#/$defs/Device"}
        ]}}})");
        write("device.json", R"({"$defs": {"Device": {
            "properties": {
                "Type": {"enum": ["First", "Second"]},
                "Name": {"default": "ignored"},
                "Rate": {"type": "number", "default": 0},
                "Enabled": {"default": false},
                "Required": {"default": 1},
                "Object": {"default": {}},
                "Mixed": {"default": [1, "two"]},
                "Ref": {"$ref": "missing.json", "default": 2},
                "Composed": {"allOf": [{"default": 3}]}
            },
            "required": ["Type", "Required"]
        }}})");
    }

    void TearDown() override
    {
        if (!directory.empty())
        {
            std::filesystem::remove_all(directory);
        }
    }

    void write(const char* file, std::string_view text)
    {
        std::ofstream stream(directory / file);
        stream << text;
        ASSERT_TRUE(stream.good());
    }
};

TEST_F(SchemaDefaultsTest, OverlaysOnlyDirectOptionalDefaults)
{
    const SchemaDefaults defaults(directory);
    const Json configured = {{"Type", "First"}};
    const Json expected = {{"Type", "First"}, {"Rate", 0}, {"Enabled", false}};
    EXPECT_EQ(defaults.exposes(configured), expected);
    EXPECT_EQ(configured.size(), 1);

    const Json explicitValues = {
        {"Type", "Second"}, {"Rate", 5}, {"Enabled", nullptr}};
    EXPECT_EQ(defaults.exposes(explicitValues), explicitValues);

    const Json unknown = {{"Type", "Unknown"}};
    EXPECT_EQ(defaults.exposes(unknown), unknown);
}

TEST_F(SchemaDefaultsTest, IgnoresDuplicateTypesAndBrokenReferences)
{
    write("exposes_record.json", R"({"$defs": {"ConfigSchema": {"oneOf": [
        {"$ref": "missing.json"},
        {"$ref": "device.json#/$defs/Device"},
        {"properties": {"Type": {"const": "Second"}}}
    ]}}})");
    const SchemaDefaults defaults(directory);
    EXPECT_EQ(defaults.exposes({{"Type", "First"}}).at("Rate"), 0);

    const Json duplicate = {{"Type", "Second"}};
    EXPECT_EQ(defaults.exposes(duplicate), duplicate);
}
} // namespace
