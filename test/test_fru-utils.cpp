#include "fru_utils.hpp"

#include <algorithm>
#include <array>
#include <iterator>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

extern "C"
{
// Include for I2C_SMBUS_BLOCK_MAX
#include <linux/i2c.h>
}

static constexpr size_t blockSize = I2C_SMBUS_BLOCK_MAX;

TEST(ValidateHeaderTest, InvalidFruVersionReturnsFalse)
{
    // Validates the FruVersion is checked for the only legal value.
    constexpr std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> fruHeader = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(validateHeader(fruHeader));
}

TEST(ValidateHeaderTest, InvalidReservedReturnsFalse)
{
    // Validates the reserved bit(7:4) of first bytes.
    constexpr std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> fruHeader = {
        0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(validateHeader(fruHeader));
}

TEST(ValidateHeaderTest, InvalidPaddingReturnsFalse)
{
    // Validates the padding byte (7th byte).
    constexpr std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> fruHeader = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};

    EXPECT_FALSE(validateHeader(fruHeader));
}

TEST(ValidateHeaderTest, InvalidChecksumReturnsFalse)
{
    // Validates the checksum, check for incorrect value.
    constexpr std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> fruHeader = {
        0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0x00};

    EXPECT_FALSE(validateHeader(fruHeader));
}

TEST(ValidateHeaderTest, ValidChecksumReturnsTrue)
{
    // Validates the checksum, check for correct value.
    constexpr std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> fruHeader = {
        0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0xf5};

    EXPECT_TRUE(validateHeader(fruHeader));
}

TEST(VerifyOffsetTest, EmptyFruDataReturnsFalse)
{
    // Validates the FruData size is checked for non empty.
    std::vector<uint8_t> fruData = {};

    EXPECT_FALSE(verifyOffset(fruData, fruAreas::fruAreaChassis, 0));
}

TEST(VerifyOffsetTest, AreaOutOfRangeReturnsFalse)
{
    // Validates the FruArea value, check if it is within range.
    const std::vector<uint8_t> fruData = {0x01, 0x00, 0x00, 0x00, 0x00,
                                          0x00, 0x00, 0x00, 0x00};

    unsigned int areaOutOfRange = 8;
    EXPECT_FALSE(
        verifyOffset(fruData, static_cast<fruAreas>(areaOutOfRange), 0));
}

TEST(VerifyOffsetTest, OverlapNextAreaReturnsFalse)
{
    // Validates the Overlap of offsets with overlapped values.
    const std::vector<uint8_t> fruData = {0x01, 0x00, 0x01, 0x02, 0x03,
                                          0x04, 0x00, 0x00, 0x00};

    EXPECT_FALSE(verifyOffset(fruData, fruAreas::fruAreaChassis, 2));
}

TEST(VerifyOffsetTest, OverlapPrevAreaReturnsFalse)
{
    // Validates the Overlap of offsets with overlapped values.
    const std::vector<uint8_t> fruData = {0x01, 0x00, 0x01, 0x03, 0x02,
                                          0x07, 0x00, 0x00, 0x00};

    EXPECT_FALSE(verifyOffset(fruData, fruAreas::fruAreaProduct, 2));
}

TEST(VerifyOffsetTest, ValidInputDataNoOverlapReturnsTrue)
{
    // Validates all inputs with expected value and no overlap.
    const std::vector<uint8_t> fruData = {0x01, 0x00, 0x01, 0x02, 0x03,
                                          0x04, 0x00, 0x00, 0x00};

    EXPECT_TRUE(verifyOffset(fruData, fruAreas::fruAreaChassis, 1));
}

TEST(VerifyChecksumTest, EmptyInput)
{
    std::vector<uint8_t> data = {};

    EXPECT_EQ(calculateChecksum(data), 0);
}

TEST(VerifyChecksumTest, SingleOneInput)
{
    std::vector<uint8_t> data(1, 1);

    EXPECT_EQ(calculateChecksum(data), 255);
}

TEST(VerifyChecksumTest, AllOneInput)
{
    std::vector<uint8_t> data(256, 1);

    EXPECT_EQ(calculateChecksum(data), 0);
}

TEST(VerifyChecksumTest, WrapBoundaryLow)
{
    std::vector<uint8_t> data = {255, 0};

    EXPECT_EQ(calculateChecksum(data), 1);
}

TEST(VerifyChecksumTest, WrapBoundaryExact)
{
    std::vector<uint8_t> data = {255, 1};

    EXPECT_EQ(calculateChecksum(data), 0);
}

TEST(VerifyChecksumTest, WrapBoundaryHigh)
{
    std::vector<uint8_t> data = {255, 2};

    EXPECT_EQ(calculateChecksum(data), 255);
}

int64_t getDataTempl(const std::vector<uint8_t>& data, off_t offset,
                     size_t length, uint8_t* outBuf)
{
    if (offset >= static_cast<off_t>(data.size()))
    {
        return 0;
    }

    uint16_t idx = offset;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    for (; idx < std::min(data.size(), offset + length); ++idx, ++outBuf)
    {
        *outBuf = data[idx];
    }

    return idx - offset;
}

TEST(FRUReaderTest, ReadData)
{
    std::vector<uint8_t> data = {};
    data.reserve(blockSize * 2);
    for (size_t i = 0; i < blockSize * 2; i++)
    {
        data.push_back(i);
    }
    std::array<uint8_t, blockSize * 2> rdbuf{};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(0, data.size(), rdbuf.data()),
              static_cast<ssize_t>(data.size()));
    EXPECT_TRUE(std::equal(rdbuf.begin(), rdbuf.end(), data.begin()));
    for (size_t i = 0; i < blockSize * 2; i++)
    {
        EXPECT_EQ(reader.read(i, 1, rdbuf.data()), 1);
        EXPECT_EQ(rdbuf[i], i);
    }
    EXPECT_EQ(reader.read(blockSize - 1, 2, rdbuf.data()), 2);
    EXPECT_EQ(rdbuf[0], blockSize - 1);
    EXPECT_EQ(rdbuf[1], blockSize);
}

TEST(FRUReaderTest, StartPastUnknownEOF)
{
    const std::vector<uint8_t> data = {};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(1, 1, nullptr), 0);
}

TEST(FRUReaderTest, StartPastKnownEOF)
{
    std::vector<uint8_t> data = {};
    data.resize(blockSize / 2);
    std::array<uint8_t, blockSize> blockData{};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(0, blockSize, blockData.data()),
              static_cast<ssize_t>(data.size()));
    EXPECT_EQ(reader.read(data.size(), 1, nullptr), 0);
    EXPECT_EQ(reader.read(data.size() + 1, 1, nullptr), 0);
    EXPECT_EQ(reader.read(blockSize, 1, nullptr), 0);
    EXPECT_EQ(reader.read(blockSize + 1, 1, nullptr), 0);
}

TEST(FRUReaderTest, DecreasingEOF)
{
    const std::vector<uint8_t> data = {};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(blockSize * 2, 1, nullptr), 0);
    EXPECT_EQ(reader.read(blockSize + (blockSize / 2), 1, nullptr), 0);
    EXPECT_EQ(reader.read(blockSize, 1, nullptr), 0);
    EXPECT_EQ(reader.read(blockSize / 2, 1, nullptr), 0);
    EXPECT_EQ(reader.read(0, 1, nullptr), 0);
}

TEST(FRUReaderTest, CacheHit)
{
    std::vector<uint8_t> data = {'X'};
    std::array<uint8_t, blockSize> read1{};
    std::array<uint8_t, blockSize> read2{};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    // cache hit should return the same data for the second read even if we
    // change it behind the FRUReader's back after the first
    EXPECT_EQ(reader.read(0, blockSize, read1.data()), 1);
    data[0] = 'Y';
    EXPECT_EQ(reader.read(0, blockSize, read2.data()), 1);
    EXPECT_EQ(read1[0], read2[0]);
}

TEST(FRUReaderTest, ReadPastKnownEnd)
{
    const std::vector<uint8_t> data = {'X', 'Y'};
    std::array<uint8_t, blockSize> rdbuf{};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(0, data.size(), rdbuf.data()),
              static_cast<ssize_t>(data.size()));
    EXPECT_EQ(rdbuf[0], 'X');
    EXPECT_EQ(rdbuf[1], 'Y');
    EXPECT_EQ(reader.read(1, data.size(), rdbuf.data()),
              static_cast<ssize_t>(data.size() - 1));
    EXPECT_EQ(rdbuf[0], 'Y');
}

TEST(FRUReaderTest, MultiBlockRead)
{
    std::vector<uint8_t> data = {};
    data.resize(blockSize, 'X');
    data.resize(2 * blockSize, 'Y');
    std::array<uint8_t, 2 * blockSize> rdbuf{};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(0, 2 * blockSize, rdbuf.data()),
              static_cast<ssize_t>(2 * blockSize));
    EXPECT_TRUE(std::equal(rdbuf.begin(), rdbuf.end(), data.begin()));
}

TEST(FRUReaderTest, ShrinkingEEPROM)
{
    std::vector<uint8_t> data = {};
    data.resize(3 * blockSize, 'X');
    std::array<uint8_t, blockSize> rdbuf{};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(data.size() - 1, 2, rdbuf.data()), 1);
    data.resize(blockSize);
    EXPECT_EQ(reader.read(data.size() - 1, 2, rdbuf.data()), 1);
}

TEST(FindFRUHeaderTest, InvalidHeader)
{
    const std::vector<uint8_t> data = {255, 16};
    off_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData{};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_FALSE(findFRUHeader(reader, "error", blockData, offset));
}

TEST(FindFRUHeaderTest, NoData)
{
    const std::vector<uint8_t> data = {};
    off_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData{};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_FALSE(findFRUHeader(reader, "error", blockData, offset));
}

TEST(FindFRUHeaderTest, ValidHeader)
{
    const std::vector<uint8_t> data = {0x01, 0x00, 0x01, 0x02,
                                       0x03, 0x04, 0x00, 0xf5};
    off_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData{};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_TRUE(findFRUHeader(reader, "error", blockData, offset));
    EXPECT_EQ(0, offset);
}

TEST(FindFRUHeaderTest, TyanInvalidHeader)
{
    std::vector<uint8_t> data = {'$', 'T', 'Y', 'A', 'N', '$', 0, 0};
    data.resize(0x6000 + I2C_SMBUS_BLOCK_MAX);
    off_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData{};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_FALSE(findFRUHeader(reader, "error", blockData, offset));
}

TEST(FindFRUHeaderTest, TyanNoData)
{
    const std::vector<uint8_t> data = {'$', 'T', 'Y', 'A', 'N', '$', 0, 0};
    off_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData{};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_FALSE(findFRUHeader(reader, "error", blockData, offset));
}

TEST(FindFRUHeaderTest, TyanValidHeader)
{
    std::vector<uint8_t> data = {'$', 'T', 'Y', 'A', 'N', '$', 0, 0};
    data.resize(0x6000);
    constexpr std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> fruHeader = {
        0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0xf5};
    copy(fruHeader.begin(), fruHeader.end(), back_inserter(data));

    off_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData{};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_TRUE(findFRUHeader(reader, "error", blockData, offset));
    EXPECT_EQ(0x6000, offset);
}

TEST(formatIPMIFRU, FullDecode)
{
    const std::array<uint8_t, 176> bmc_fru = {
        0x01, 0x00, 0x00, 0x01, 0x0b, 0x00, 0x00, 0xf3, 0x01, 0x0a, 0x19, 0x1f,
        0x0f, 0xe6, 0xc6, 0x4e, 0x56, 0x49, 0x44, 0x49, 0x41, 0xc5, 0x50, 0x33,
        0x38, 0x30, 0x39, 0xcd, 0x31, 0x35, 0x38, 0x33, 0x33, 0x32, 0x34, 0x38,
        0x30, 0x30, 0x31, 0x35, 0x30, 0xd2, 0x36, 0x39, 0x39, 0x2d, 0x31, 0x33,
        0x38, 0x30, 0x39, 0x2d, 0x30, 0x34, 0x30, 0x34, 0x2d, 0x36, 0x30, 0x30,
        0xc0, 0x01, 0x01, 0xd6, 0x4d, 0x41, 0x43, 0x3a, 0x20, 0x33, 0x43, 0x3a,
        0x36, 0x44, 0x3a, 0x36, 0x36, 0x3a, 0x31, 0x34, 0x3a, 0x43, 0x38, 0x3a,
        0x37, 0x41, 0xc1, 0x3b, 0x01, 0x09, 0x19, 0xc6, 0x4e, 0x56, 0x49, 0x44,
        0x49, 0x41, 0xc9, 0x50, 0x33, 0x38, 0x30, 0x39, 0x2d, 0x42, 0x4d, 0x43,
        0xd2, 0x36, 0x39, 0x39, 0x2d, 0x31, 0x33, 0x38, 0x30, 0x39, 0x2d, 0x30,
        0x34, 0x30, 0x34, 0x2d, 0x36, 0x30, 0x30, 0xc4, 0x41, 0x45, 0x2e, 0x31,
        0xcd, 0x31, 0x35, 0x38, 0x33, 0x33, 0x32, 0x34, 0x38, 0x30, 0x30, 0x31,
        0x35, 0x30, 0xc0, 0xc4, 0x76, 0x30, 0x2e, 0x31, 0xc1, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xb4, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    boost::container::flat_map<std::string, std::string> result;
    ASSERT_EQ(formatIPMIFRU(bmc_fru, result), resCodes::resOK);

    EXPECT_THAT(
        result,
        UnorderedElementsAre(
            Pair("BOARD_FRU_VERSION_ID", ""), Pair("BOARD_INFO_AM1", "01"),
            Pair("BOARD_INFO_AM2", "MAC: 3C:6D:66:14:C8:7A"),
            Pair("BOARD_LANGUAGE_CODE", "25"),
            Pair("BOARD_MANUFACTURER", "NVIDIA"),
            Pair("BOARD_MANUFACTURE_DATE", "20240831T055100Z"),
            Pair("BOARD_PART_NUMBER", "699-13809-0404-600"),
            Pair("BOARD_PRODUCT_NAME", "P3809"),
            Pair("BOARD_SERIAL_NUMBER", "1583324800150"),
            Pair("Common_Format_Version", "1"), Pair("PRODUCT_ASSET_TAG", ""),
            Pair("MAC_BOARD_INFO_AM2", "3C:6D:66:14:C8:7A"),
            Pair("PRODUCT_FRU_VERSION_ID", "v0.1"),
            Pair("PRODUCT_LANGUAGE_CODE", "25"),
            Pair("PRODUCT_MANUFACTURER", "NVIDIA"),
            Pair("PRODUCT_PART_NUMBER", "699-13809-0404-600"),
            Pair("PRODUCT_PRODUCT_NAME", "P3809-BMC"),
            Pair("PRODUCT_SERIAL_NUMBER", "1583324800150"),
            Pair("PRODUCT_VERSION", "AE.1")));
}
