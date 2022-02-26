#include "FruUtils.hpp"

#include <algorithm>
#include <array>
#include <iterator>

#include "gtest/gtest.h"

static constexpr size_t blockSize = I2C_SMBUS_BLOCK_MAX;

TEST(ValidateIPMIHeaderTest, InvalidFruVersionReturnsFalse)
{
    // Validates the FruVersion is checked for the only legal value.
    const std::vector<uint8_t> fruHeader = {0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(validateIPMIHeader(fruHeader));
}

TEST(ValidateIPMIHeaderTest, InvalidReservedReturnsFalse)
{
    // Validates the reserved bit(7:4) of first bytes.
    const std::vector<uint8_t> fruHeader = {0xf0, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(validateIPMIHeader(fruHeader));
}

TEST(ValidateIPMIHeaderTest, InvalidPaddingReturnsFalse)
{
    // Validates the padding byte (7th byte).
    const std::vector<uint8_t> fruHeader = {0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x01, 0x00};

    EXPECT_FALSE(validateIPMIHeader(fruHeader));
}

TEST(ValidateIPMIHeaderTest, InvalidChecksumReturnsFalse)
{
    // Validates the checksum, check for incorrect value.
    const std::vector<uint8_t> fruHeader = {0x01, 0x00, 0x01, 0x02,
                                            0x03, 0x04, 0x00, 0x00};

    EXPECT_FALSE(validateIPMIHeader(fruHeader));
}

TEST(ValidateIPMIHeaderTest, ValidChecksumReturnsTrue)
{
    // Validates the checksum, check for correct value.
    const std::vector<uint8_t> fruHeader = {0x01, 0x00, 0x01, 0x02,
                                            0x03, 0x04, 0x00, 0xf5};

    EXPECT_TRUE(validateIPMIHeader(fruHeader));
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

    uint16_t idx;
    for (idx = offset; idx < data.size() && idx < offset + length;
         ++idx, ++outBuf)
    {
        *outBuf = data[idx];
    }

    return idx - offset;
}

TEST(FRUReaderTest, ReadData)
{
    std::vector<uint8_t> data = {};
    for (size_t i = 0; i < blockSize * 2; i++)
    {
        data.push_back(i);
    }
    std::array<uint8_t, blockSize * 2> rdbuf;
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
    std::array<uint8_t, blockSize> blockData;
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
    std::array<uint8_t, blockSize> read1, read2;
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
    std::array<uint8_t, blockSize> rdbuf;
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
    std::array<uint8_t, 2 * blockSize> rdbuf;
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
    std::array<uint8_t, blockSize> rdbuf;
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(data.size() - 1, 2, rdbuf.data()), 1);
    data.resize(blockSize);
    EXPECT_EQ(reader.read(data.size() - 1, 2, rdbuf.data()), 1);
}

static constexpr auto testFRUData = std::to_array<uint8_t>({
    0x01, 0x00, 0x01, 0x05, 0x0d, 0x00, 0x00, 0xec, 0x01, 0x04, 0x11, 0xd1,
    0x4f, 0x42, 0x4d, 0x43, 0x5f, 0x54, 0x45, 0x53, 0x54, 0x5f, 0x43, 0x48,
    0x41, 0x53, 0x53, 0x49, 0x53, 0xc6, 0x58, 0x59, 0x5a, 0x31, 0x32, 0x33,
    0xc1, 0x00, 0x00, 0xc4, 0x01, 0x08, 0x19, 0xd8, 0xe7, 0xd1, 0xcf, 0x4f,
    0x70, 0x65, 0x6e, 0x42, 0x4d, 0x43, 0x20, 0x50, 0x72, 0x6f, 0x6a, 0x65,
    0x63, 0x74, 0xcf, 0x4f, 0x42, 0x4d, 0x43, 0x5f, 0x54, 0x45, 0x53, 0x54,
    0x5f, 0x42, 0x4f, 0x41, 0x52, 0x44, 0xc6, 0x41, 0x42, 0x43, 0x30, 0x31,
    0x32, 0xcd, 0x42, 0x4f, 0x41, 0x52, 0x44, 0x5f, 0x50, 0x41, 0x52, 0x54,
    0x4e, 0x55, 0x4d, 0xc0, 0xc1, 0x00, 0x00, 0x73, 0x01, 0x06, 0x19, 0xcf,
    0x4f, 0x70, 0x65, 0x6e, 0x42, 0x4d, 0x43, 0x20, 0x50, 0x72, 0x6f, 0x6a,
    0x65, 0x63, 0x74, 0xd1, 0x4f, 0x42, 0x4d, 0x43, 0x5f, 0x54, 0x45, 0x53,
    0x54, 0x5f, 0x50, 0x52, 0x4f, 0x44, 0x55, 0x43, 0x54, 0xc0, 0xc0, 0xc4,
    0x39, 0x39, 0x39, 0x39, 0xc0, 0xc0, 0xc1, 0x3c,
});

TEST(ReadFRUContentsTest, InvalidHeader)
{
    const std::vector<uint8_t> data = {255, 16};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_TRUE(readFRUContents(reader, "error").empty());
}

TEST(ReadFRUContentsTest, NoData)
{
    const std::vector<uint8_t> data = {};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_TRUE(readFRUContents(reader, "error").empty());
}

TEST(ReadFRUContentsTest, IPMIValidData)
{
    const std::vector<uint8_t> data(testFRUData.begin(), testFRUData.end());
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(readFRUContents(reader, "error"), data);
}

TEST(ReadFRUContentsTest, IPMIInvalidData)
{
    std::vector<uint8_t> data(testFRUData.begin(), testFRUData.end());
    // corrupt a byte to throw off the checksum
    data[7] ^= 0xff;
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_TRUE(readFRUContents(reader, "error").empty());
}

TEST(ReadFRUContentsTest, TyanInvalidHeader)
{
    std::vector<uint8_t> data = {'$', 'T', 'Y', 'A', 'N', '$', 0, 0};
    data.resize(0x6000 + 32);
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_TRUE(readFRUContents(reader, "error").empty());
}

TEST(ReadFRUContentsTest, TyanNoData)
{
    const std::vector<uint8_t> data = {'$', 'T', 'Y', 'A', 'N', '$', 0, 0};
    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    EXPECT_TRUE(readFRUContents(reader, "error").empty());
}

TEST(ReadFRUContentsTest, TyanValidData)
{
    std::vector<uint8_t> data = {'$', 'T', 'Y', 'A', 'N', '$', 0, 0};
    data.resize(0x6000);
    copy(testFRUData.begin(), testFRUData.end(), back_inserter(data));

    auto getData = [&data](auto o, auto l, auto* b) {
        return getDataTempl(data, o, l, b);
    };
    FRUReader reader(getData);

    std::vector<uint8_t> device = readFRUContents(reader, "error");
    EXPECT_TRUE(std::equal(device.begin(), device.end(), testFRUData.begin()));
}
