// SPDX-License-Identifier: Apache-2.0

#include "fru_device/blocklist.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

#include <gtest/gtest.h>

class BlocklistTest : public ::testing::Test
{
  protected:
    std::filesystem::path dir;
    std::filesystem::path file;

    void SetUp() override
    {
        const auto pattern = std::filesystem::temp_directory_path() /
                             "fru-blocklist-test-XXXXXX";
        auto path = pattern.string();
        ASSERT_NE(mkdtemp(path.data()), nullptr);
        dir = path;
        file = dir / "blacklist.json";
    }

    void TearDown() override
    {
        std::error_code ec;
        std::filesystem::remove(file, ec);
        std::filesystem::remove(dir, ec);
    }

    void writeFile(std::string_view content)
    {
        std::ofstream stream(file);
        stream << content;
        stream.close();
        ASSERT_TRUE(stream);
    }
};

TEST_F(BlocklistTest, MissingFileReturnsEmptyBlocklist)
{
    const auto result = loadBlocklist(file.c_str());
    if (!result)
    {
        throw std::invalid_argument("no value");
    }
    EXPECT_TRUE(result->buses.empty());
    EXPECT_TRUE(result->addresses.empty());
}

TEST_F(BlocklistTest, LoadsBusAndAddressEntries)
{
    ASSERT_NO_FATAL_FAILURE(writeFile(
        R"({"buses":[1,{"bus":3,"addresses":["0x30","0x40"]}],"addresses":["0x71"]})"));

    const auto result = loadBlocklist(file.c_str());
    if (!result)
    {
        throw std::invalid_argument("no value");
    }
    ASSERT_TRUE(result->buses.contains(1));
    EXPECT_FALSE(result->buses.at(1));
    ASSERT_TRUE(result->buses.contains(3));

    const auto& blockedAddresses = result->buses.at(3);
    if (!blockedAddresses)
    {
        throw std::invalid_argument("no value");
    }
    EXPECT_EQ(*blockedAddresses, (std::flat_set<size_t>{0x30, 0x40}));
    EXPECT_EQ(result->addresses, std::set<size_t>{0x71});
}

TEST_F(BlocklistTest, InvalidContentsReturnFailure)
{
    constexpr std::array invalidContents{
        "{",
        "[]",
        R"({"buses":{}})",
        R"({"buses":[1,"3"]})",
        R"({"buses":[{"bus":3,"addresses":[48]}]})",
        R"({"buses":[1],"addresses":["0xZZ"]})",
    };

    for (const auto* content : invalidContents)
    {
        SCOPED_TRACE(content);
        ASSERT_NO_FATAL_FAILURE(writeFile(content));
        EXPECT_FALSE(loadBlocklist(file.c_str()));
    }
}
