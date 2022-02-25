#include "FruUtils.hpp"

#include <array>
#include<algorithm>
#include<iterator>

#include "gtest/gtest.h"

extern "C"
{
// Include for I2C_SMBUS_BLOCK_MAX
#include <linux/i2c.h>
}

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

int64_t getDataTempl(
    const std::vector<uint8_t>& data,
    uint16_t offset, uint8_t length, uint8_t* outBuf)
{
    if (offset >= data.size())
    {
        return 0;
    }

    uint16_t idx;
    for (idx = offset;
         idx < data.size() && idx < offset + length;
         ++idx, ++outBuf)
    {
        *outBuf = data[idx];
    }

    return idx - offset;
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
    constexpr size_t blksz = FRUReader::cacheBlockSize;
    data.resize(blksz / 2);
    std::array<uint8_t, blksz> blockData;
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(0, blksz, blockData.data()), data.size());
    EXPECT_EQ(reader.read(data.size(), 1, nullptr), 0);
    EXPECT_EQ(reader.read(data.size() + 1, 1, nullptr), 0);
    EXPECT_EQ(reader.read(blksz, 1, nullptr), 0);
    EXPECT_EQ(reader.read(blksz + 1, 1, nullptr), 0);
}

TEST(FRUReaderTest, DecreasingEOF)
{
    const std::vector<uint8_t> data = {};
    constexpr size_t blksz = FRUReader::cacheBlockSize;
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(blksz * 2, 1, nullptr), 0);
    EXPECT_EQ(reader.read(blksz + (blksz / 2), 1, nullptr), 0);
    EXPECT_EQ(reader.read(blksz, 1, nullptr), 0);
    EXPECT_EQ(reader.read(blksz / 2, 1, nullptr), 0);
    EXPECT_EQ(reader.read(0, 1, nullptr), 0);
}

TEST(FRUReaderTest, CacheHit)
{
    std::vector<uint8_t> data = {'X'};
    constexpr size_t blksz = FRUReader::cacheBlockSize;
    std::array<uint8_t, blksz> read1, read2;
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    // cache hit should return the same data for the second read even if we
    // change it behind the FRUReader's back after the first
    EXPECT_EQ(reader.read(0, blksz, read1.data()), 1);
    data[0] = 'Y';
    EXPECT_EQ(reader.read(0, blksz, read2.data()), 1);
    EXPECT_EQ(read1[0], read2[0]);
}

TEST(FRUReaderTest, ReadPastKnownEnd)
{
    const std::vector<uint8_t> data = {'X', 'Y'};
    std::array<uint8_t, FRUReader::cacheBlockSize> rdbuf;
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(0, data.size(), rdbuf.data()), data.size());
    EXPECT_EQ(rdbuf[0], 'X');
    EXPECT_EQ(rdbuf[1], 'Y');
    EXPECT_EQ(reader.read(1, data.size(), rdbuf.data()), data.size() - 1);
    EXPECT_EQ(rdbuf[0], 'Y');
}

TEST(FRUReaderTest, MultiBlockRead)
{
    constexpr size_t blksz = FRUReader::cacheBlockSize;
    std::vector<uint8_t> data = {};
    data.resize(blksz, 'X');
    data.resize(2 * blksz, 'Y');
    std::array<uint8_t, 2 * blksz> rdbuf;
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(0, 2 * blksz, rdbuf.data()), 2 * blksz);
    EXPECT_TRUE(std::equal(rdbuf.begin(), rdbuf.end(), data.begin()));
}

TEST(FRUReaderTest, ShrinkingEEPROM)
{
    constexpr size_t blksz = FRUReader::cacheBlockSize;
    std::vector<uint8_t> data = {};
    data.resize(3 * blksz, 'X');
    std::array<uint8_t, blksz> rdbuf;
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(data.size() - 1, 2, rdbuf.data()), 1);
    data.resize(blksz);
    EXPECT_EQ(reader.read(data.size() - 1, 2, rdbuf.data()), 1);
}

TEST(FindFRUHeaderTest, InvalidHeader)
{
    const std::vector<uint8_t> data = {255, 16};
    uint16_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_FALSE(findFRUHeader(reader, "error", blockData, offset));
}

TEST(FindFRUHeaderTest, NoData)
{
    const std::vector<uint8_t> data = {};
    uint16_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_FALSE(findFRUHeader(reader, "error", blockData, offset));
}

TEST(FindFRUHeaderTest, ValidHeader)
{
    const std::vector<uint8_t> data = {
        0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0xf5};
    uint16_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;
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
    uint16_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_FALSE(findFRUHeader(reader, "error", blockData, offset));
}

TEST(FindFRUHeaderTest, TyanNoData)
{
    const std::vector<uint8_t> data = {'$', 'T', 'Y', 'A', 'N', '$', 0, 0};
    uint16_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;
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

    uint16_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_TRUE(findFRUHeader(reader, "error", blockData, offset));
    EXPECT_EQ(0x6000, offset);
}
