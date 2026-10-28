#include "entity_manager/schema_dbus_types.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <flat_map>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

using schema_dbus_types::DBusValue;
using schema_dbus_types::SchemaDBusTypes;
using schema_dbus_types::toDBusValue;

TEST(ToDBusValue, ParsesNumericStringsLikeTemplateSubstitution)
{
    EXPECT_EQ(toDBusValue("uint64", "0x1d"), DBusValue{uint64_t{0x1d}});
    EXPECT_EQ(toDBusValue("uint64", "15"), DBusValue{uint64_t{15}});
    EXPECT_EQ(toDBusValue("uint64", 15), DBusValue{uint64_t{15}});
}

TEST(ToDBusValue, RejectsStringThatIsNotANumber)
{
    // A template missing its "$" is still a string once probed.
    EXPECT_EQ(toDBusValue("uint64", "address"), std::nullopt);
    // Zeroed out FRU fields are not taken as numbers.
    EXPECT_EQ(toDBusValue("uint64", "0012"), std::nullopt);
}

TEST(ToDBusValue, RejectsOutOfRangeIntegers)
{
    EXPECT_EQ(toDBusValue("byte", 255), DBusValue{uint8_t{255}});
    EXPECT_EQ(toDBusValue("byte", 256), std::nullopt);
    EXPECT_EQ(toDBusValue("uint64", -1), std::nullopt);
    EXPECT_EQ(toDBusValue("int64", -1), DBusValue{int64_t{-1}});
}

TEST(ToDBusValue, RejectsFractionForInteger)
{
    EXPECT_EQ(toDBusValue("uint64", 1.5), std::nullopt);
    EXPECT_EQ(toDBusValue("double", 1.5), DBusValue{1.5});
    EXPECT_EQ(toDBusValue("double", 2), DBusValue{2.0});
}

TEST(ToDBusValue, AcceptsWholeNumberWrittenAsDouble)
{
    // A settable number is written back to the configuration as a double.
    EXPECT_EQ(toDBusValue("uint64", 3.0), DBusValue{uint64_t{3}});
    EXPECT_EQ(toDBusValue("int64", -2.0), DBusValue{int64_t{-2}});
    EXPECT_EQ(toDBusValue("byte", 255.0), DBusValue{uint8_t{255}});
    EXPECT_EQ(toDBusValue("byte", 256.0), std::nullopt);
    EXPECT_EQ(toDBusValue("uint64", -1.0), std::nullopt);
}

TEST(ToDBusValue, KeepsStringsAndBooleansStrict)
{
    EXPECT_EQ(toDBusValue("string", "name"), DBusValue{std::string("name")});
    EXPECT_EQ(toDBusValue("string", 5), std::nullopt);
    EXPECT_EQ(toDBusValue("boolean", true), DBusValue{true});
    EXPECT_EQ(toDBusValue("boolean", "true"), std::nullopt);
}

TEST(ToDBusValue, ConvertsArraysElementByElement)
{
    const nlohmann::json pid = {7, 43, 0, 0, 0, 1};
    const DBusValue bytes = std::vector<uint8_t>{7, 43, 0, 0, 0, 1};
    const DBusValue integers = std::vector<uint64_t>{7, 43, 0, 0, 0, 1};
    EXPECT_EQ(toDBusValue("array[byte]", pid), bytes);
    EXPECT_EQ(toDBusValue("array[uint64]", pid), integers);

    const nlohmann::json badElement = {7, "pid", 0};
    EXPECT_EQ(toDBusValue("array[byte]", badElement), std::nullopt);
    EXPECT_EQ(toDBusValue("array[byte]", 7), std::nullopt);
}

TEST(ToDBusValue, RejectsUnknownTypes)
{
    EXPECT_EQ(toDBusValue("uint32", 7), std::nullopt);
    EXPECT_EQ(toDBusValue("array[boolean]", nlohmann::json::array()),
              std::nullopt);
}

namespace
{

const std::flat_map<std::string, nlohmann::json, std::less<>> noSchemas;

} // namespace

TEST(SchemaDBusTypes, FindsTypesForEveryRecordTypeOfADefinition)
{
    const nlohmann::json schema = nlohmann::json::parse(R"({
        "$defs": {
            "Target": {
                "properties": {
                    "Address": {"type": "string", "x-dbus-type": "uint64"},
                    "Name": {"type": "string"},
                    "Type": {"enum": ["TargetA", "TargetB"]}
                }
            },
            "Other": {
                "properties": {
                    "Address": {"type": "string"},
                    "Type": {"const": "Other"}
                }
            }
        }
    })");

    SchemaDBusTypes types;
    types.addSchema(schema, "target.json", noSchemas);

    EXPECT_EQ(types.find("TargetA", "Address"), "uint64");
    EXPECT_EQ(types.find("TargetB", "Address"), "uint64");
    EXPECT_EQ(types.find("TargetA", "Name"), std::nullopt);
    EXPECT_EQ(types.find("Other", "Address"), std::nullopt);
    EXPECT_EQ(types.find("Unknown", "Address"), std::nullopt);
}

TEST(SchemaDBusTypes, IgnoresUnknownDBusTypes)
{
    const nlohmann::json schema = nlohmann::json::parse(R"({
        "$defs": {
            "Target": {
                "properties": {
                    "Address": {"type": "string", "x-dbus-type": "uint32"},
                    "Bus": {"type": "number", "x-dbus-type": "uint64"},
                    "Flags": {"type": "array", "x-dbus-type": "array[boolean]"},
                    "Type": {"const": "Target"}
                }
            }
        }
    })");

    SchemaDBusTypes types;
    types.addSchema(schema, "target.json", noSchemas);

    EXPECT_EQ(types.find("Target", "Address"), std::nullopt);
    EXPECT_EQ(types.find("Target", "Flags"), std::nullopt);
    EXPECT_EQ(types.find("Target", "Bus"), "uint64");
}

TEST(SchemaDBusTypes, FollowsReferencesToOtherSchemas)
{
    const std::filesystem::path dir =
        std::filesystem::temp_directory_path() / "test_schema_dbus_types";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir / "platform");

    std::ofstream(dir / "common.json") << R"({
        "$defs": {"Pid": {"type": "array", "x-dbus-type": "array[byte]"}}
    })";
    std::ofstream(dir / "platform" / "target.json") << R"({
        "$defs": {
            "Target": {
                "properties": {
                    "Address": {"$ref": "common.json#/$defs/Pid"},
                    "Bus": {"$ref": "#/$defs/Bus"},
                    "Type": {"const": "Target"}
                }
            },
            "Bus": {"type": "number", "x-dbus-type": "uint64"}
        }
    })";
    // Comments are allowed in schemas, and a broken one is skipped.
    std::ofstream(dir / "broken.json") << "{ // not closed";

    const SchemaDBusTypes types(dir);

    EXPECT_EQ(types.find("Target", "Address"), "array[byte]");
    EXPECT_EQ(types.find("Target", "Bus"), "uint64");

    std::filesystem::remove_all(dir);
}

TEST(SchemaDBusTypes, MissingDirectoryNamesNoTypes)
{
    const SchemaDBusTypes types("/nonexistent/schemas");

    EXPECT_EQ(types.find("Target", "Address"), std::nullopt);
}
