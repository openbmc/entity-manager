#include "FruUtils.hpp"
#include "Utils.hpp"

#include <boost/asio/io_service.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <array>
#include<algorithm>
#include<iterator>

#include "gtest/gtest.h"

extern "C"
{
// Include for I2C_SMBUS_BLOCK_MAX
#include <i2c/smbus.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
}

boost::asio::io_service io;

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
    [[maybe_unused]] int flag,
    [[maybe_unused]] int file,
    [[maybe_unused]] uint16_t address,
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

TEST(FindFRUHeaderTest, InvalidHeader)
{
    const std::vector<uint8_t> data = {255, 16};
    uint16_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;
    auto getData = [&data](auto fl, auto fi, auto a, auto o, auto l,
        auto* b) { return getDataTempl(data, fl, fi, a, o, l, b); };

    EXPECT_FALSE(findFRUHeader(0, 0, 0, getData, "error", blockData, offset));
}

TEST(FindFRUHeaderTest, NoData)
{
    const std::vector<uint8_t> data = {};
    uint16_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;
    auto getData = [&data](auto fl, auto fi, auto a, auto o, auto l,
        auto* b) { return getDataTempl(data, fl, fi, a, o, l, b); };

    EXPECT_FALSE(findFRUHeader(0, 0, 0, getData, "error", blockData, offset));
}

TEST(FindFRUHeaderTest, ValidHeader)
{
    const std::vector<uint8_t> data = {
        0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0xf5};
    uint16_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;
    auto getData = [&data](auto fl, auto fi, auto a, auto o, auto l,
        auto* b) { return getDataTempl(data, fl, fi, a, o, l, b); };

    EXPECT_TRUE(findFRUHeader(0, 0, 0, getData, "error", blockData, offset));
    EXPECT_EQ(0, offset);
}

TEST(FindFRUHeaderTest, TyanInvalidHeader)
{
    std::vector<uint8_t> data = {'$', 'T', 'Y', 'A', 'N', '$', 0, 0};
    data.resize(0x6000 + I2C_SMBUS_BLOCK_MAX);
    uint16_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;
    auto getData = [&data](auto fl, auto fi, auto a, auto o, auto l,
        auto* b) { return getDataTempl(data, fl, fi, a, o, l, b); };

    EXPECT_FALSE(findFRUHeader(0, 0, 0, getData, "error", blockData, offset));
}

TEST(FindFRUHeaderTest, TyanNoData)
{
    const std::vector<uint8_t> data = {'$', 'T', 'Y', 'A', 'N', '$', 0, 0};
    uint16_t offset = 0;
    std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> blockData;
    auto getData = [&data](auto fl, auto fi, auto a, auto o, auto l,
        auto* b) { return getDataTempl(data, fl, fi, a, o, l, b); };

    EXPECT_FALSE(findFRUHeader(0, 0, 0, getData, "error", blockData, offset));
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
    auto getData = [&data](auto fl, auto fi, auto a, auto o, auto l,
        auto* b) { return getDataTempl(data, fl, fi, a, o, l, b); };

    EXPECT_TRUE(findFRUHeader(0, 0, 0, getData, "error", blockData, offset));
    EXPECT_EQ(0x6000, offset);
}
