#include "fru_device/gzip_utils.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <ranges>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(GzipUtils, EmptyCompressed)
{
    std::vector<uint8_t> compressed = {};
    std::optional<std::string> uncompressed = gzipInflate(compressed);
    EXPECT_EQ(uncompressed, std::nullopt);
}

TEST(GzipUtils, GoodCompressed)
{
    // Empty file created with
    // touch foo && gzip -c foo | xxd -i
    std::array<uint8_t, 24> emptyCompressed{
        0x1f, 0x8b, 0x08, 0x08, 0x16, 0x37, 0xdc, 0x68, 0x00, 0x03, 0x66, 0x6f,
        0x6f, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    std::optional<std::string> uncompressed = gzipInflate(emptyCompressed);
    ASSERT_NE(uncompressed, std::nullopt);
    if (uncompressed)
    {
        EXPECT_EQ(uncompressed->size(), 0U);
    }
}

TEST(GzipUtils, 100kcompressedZeros)
{
    // File created with
    // dd if=/dev/zero of=10kfile bs=10k count=1 && gzip -c 10kfile | xxd -i
    std::array<uint8_t, 53> emptyCompressed{
        0x1f, 0x8b, 0x08, 0x08, 0xcb, 0x37, 0xdc, 0x68, 0x00, 0x03, 0x31,
        0x30, 0x6b, 0x66, 0x69, 0x6c, 0x65, 0x00, 0xed, 0xc1, 0x01, 0x0d,
        0x00, 0x00, 0x00, 0xc2, 0xa0, 0xf7, 0x4f, 0x6d, 0x0e, 0x37, 0xa0,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x37,
        0x03, 0x9a, 0xde, 0x1d, 0x27, 0x00, 0x28, 0x00, 0x00};
    std::optional<std::string> uncompressed = gzipInflate(emptyCompressed);
    ASSERT_NE(uncompressed, std::nullopt);
    if (uncompressed)
    {
        EXPECT_EQ(uncompressed->size(), 10240U);
        EXPECT_TRUE(std::ranges::all_of(*uncompressed, [](uint8_t c) {
            return c == 0U;
        }));
    }
}

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(GzipUtils, getNodeFromXml)
{
    constexpr std::string_view xml = R"(<?xml version="1.0" encoding="UTF-8"?>
        <bookstore>
          <book category="cooking">
            <title lang="en">Everyday Italian</title>
            <author>Giada De Laurentiis</author>
            <year>2005</year>
            <price>30.00</price>
          </book>
        </bookstore>
    )";

    EXPECT_THAT(getNodeFromXml(xml, "/bookstore/book/title"),
                ElementsAre("Everyday Italian"));
    EXPECT_THAT(getNodeFromXml(xml, "/bookstore/book/author"),
                ElementsAre("Giada De Laurentiis"));
    EXPECT_THAT(getNodeFromXml(xml, "/bookstore/book/year"),
                ElementsAre("2005"));
    EXPECT_THAT(getNodeFromXml(xml, "/bookstore/book/price"),
                ElementsAre("30.00"));
    EXPECT_THAT(getNodeFromXml(xml, "//book/title"),
                ElementsAre("Everyday Italian"));
    EXPECT_THAT(getNodeFromXml(xml, "//book/author"),
                ElementsAre("Giada De Laurentiis"));
    EXPECT_THAT(getNodeFromXml(xml, "//book/year"), ElementsAre("2005"));
    EXPECT_THAT(getNodeFromXml(xml, "//book/price"), ElementsAre("30.00"));
    EXPECT_THAT(getNodeFromXml(xml, "///title"),
                ElementsAre("Everyday Italian"));
    EXPECT_THAT(getNodeFromXml(xml, "///author"),
                ElementsAre("Giada De Laurentiis"));
    EXPECT_THAT(getNodeFromXml(xml, "///year"), ElementsAre("2005"));
    EXPECT_THAT(getNodeFromXml(xml, "///price"), ElementsAre("30.00"));

    EXPECT_THAT(getNodeFromXml(xml, "/bookstore/book/noexist"), IsEmpty());
    EXPECT_THAT(getNodeFromXml(xml, "/bookstore/noexist/title"), IsEmpty());
    EXPECT_THAT(getNodeFromXml(xml, "/noexist/book/title"), IsEmpty());
    EXPECT_THAT(getNodeFromXml(xml, "//book/noexist"), IsEmpty());
    EXPECT_THAT(getNodeFromXml(xml, "//noexist/title"), IsEmpty());
    EXPECT_THAT(getNodeFromXml(xml, "///noexist"), IsEmpty());

    // invalid xpath
    EXPECT_THAT(getNodeFromXml(xml, "foo"), IsEmpty());
    EXPECT_THAT(getNodeFromXml(xml, "foo/bar"), IsEmpty());
    EXPECT_THAT(getNodeFromXml(xml, "?"), IsEmpty());
}
