#include "fru_device/fru_utils.hpp"

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

int64_t getDataTempl(std::span<const uint8_t> data, off_t offset,
                     std::span<uint8_t> outbuf)
{
    if (offset >= static_cast<off_t>(data.size()))
    {
        return 0;
    }

    size_t size = std::min(data.size() - offset, outbuf.size());
    std::memcpy(outbuf.data(), data.subspan(offset).data(), size);
    return size;
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
    std::span<uint8_t> rdbufSpan(rdbuf);
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(0, rdbufSpan), static_cast<ssize_t>(data.size()));
    EXPECT_TRUE(std::equal(rdbuf.begin(), rdbuf.end(), data.begin()));
    for (size_t i = 0; i < blockSize * 2; i++)
    {
        EXPECT_EQ(reader.read(i, rdbufSpan.subspan(0, 1)), 1);
        EXPECT_EQ(rdbuf[i], i);
    }
    EXPECT_EQ(reader.read(blockSize - 1, rdbufSpan.subspan(0, 2)), 2);
    EXPECT_EQ(rdbuf[0], blockSize - 1);
    EXPECT_EQ(rdbuf[1], blockSize);
}

TEST(FRUReaderTest, StartPastUnknownEOF)
{
    const std::vector<uint8_t> data = {};
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);
    std::array<uint8_t, 0> buf{};

    EXPECT_EQ(reader.read(1, buf), 0);
}

TEST(FRUReaderTest, StartPastKnownEOF)
{
    std::vector<uint8_t> data = {};
    data.resize(blockSize / 2);
    std::array<uint8_t, blockSize> blockData{};
    std::array<uint8_t, 0> emptyBuf{};
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(0, blockData), static_cast<ssize_t>(data.size()));
    EXPECT_EQ(reader.read(data.size(), emptyBuf), 0);
    EXPECT_EQ(reader.read(data.size() + 1, emptyBuf), 0);
    EXPECT_EQ(reader.read(blockSize, emptyBuf), 0);
    EXPECT_EQ(reader.read(blockSize + 1, emptyBuf), 0);
}

TEST(FRUReaderTest, DecreasingEOF)
{
    const std::vector<uint8_t> data = {};
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);
    std::vector<uint8_t> buf;
    EXPECT_EQ(reader.read(blockSize * 2, buf), 0);
    EXPECT_EQ(reader.read(blockSize + (blockSize / 2), buf), 0);
    EXPECT_EQ(reader.read(blockSize, buf), 0);
    EXPECT_EQ(reader.read(blockSize / 2, buf), 0);
    EXPECT_EQ(reader.read(0, buf), 0);
}

TEST(FRUReaderTest, CacheHit)
{
    std::vector<uint8_t> data = {'X'};
    std::array<uint8_t, blockSize> read1{};
    std::array<uint8_t, blockSize> read2{};
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);

    // cache hit should return the same data for the second read even if we
    // change it behind the FRUReader's back after the first
    EXPECT_EQ(reader.read(0, read1), 1);
    data[0] = 'Y';
    EXPECT_EQ(reader.read(0, read2), 1);
    EXPECT_EQ(read1[0], read2[0]);
}

TEST(FRUReaderTest, ReadPastKnownEnd)
{
    const std::vector<uint8_t> data = {'X', 'Y'};
    std::array<uint8_t, blockSize> rdbuf{};
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(0, rdbuf), static_cast<ssize_t>(data.size()));
    EXPECT_EQ(rdbuf[0], 'X');
    EXPECT_EQ(rdbuf[1], 'Y');
    EXPECT_EQ(reader.read(1, rdbuf), static_cast<ssize_t>(data.size() - 1));
    EXPECT_EQ(rdbuf[0], 'Y');
}

TEST(FRUReaderTest, MultiBlockRead)
{
    std::vector<uint8_t> data = {};
    data.resize(blockSize, 'X');
    data.resize(2 * blockSize, 'Y');
    std::array<uint8_t, 2 * blockSize> rdbuf{};
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(0, rdbuf), static_cast<ssize_t>(2 * blockSize));
    EXPECT_TRUE(std::equal(rdbuf.begin(), rdbuf.end(), data.begin()));
}

TEST(FRUReaderTest, ShrinkingEEPROM)
{
    std::vector<uint8_t> data = {};
    data.resize(3 * blockSize, 'X');
    std::array<uint8_t, blockSize> rdbuf{};
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);

    EXPECT_EQ(reader.read(data.size() - 1, rdbuf), 1);
    data.resize(blockSize);
    EXPECT_EQ(reader.read(data.size() - 1, rdbuf), 1);
}

TEST(FindFRUHeaderTest, InvalidHeader)
{
    const std::vector<uint8_t> data = {255, 16};
    off_t offset = 0;
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);
    auto sections = findFRUHeader(reader, "error", offset);
    EXPECT_EQ(sections, std::nullopt);
}

TEST(FindFRUHeaderTest, NoData)
{
    const std::vector<uint8_t> data = {};
    off_t offset = 0;
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);

    auto sections = findFRUHeader(reader, "error", offset);
    EXPECT_EQ(sections, std::nullopt);
}

TEST(FindFRUHeaderTest, ValidHeader)
{
    const std::vector<uint8_t> data = {0x01, 0x00, 0x01, 0x02,
                                       0x03, 0x04, 0x00, 0xf5};
    off_t offset = 0;
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);
    auto sections = findFRUHeader(reader, "error", offset);
    ASSERT_NE(sections, std::nullopt);
    if (sections)
    {
        EXPECT_EQ(0, sections->IpmiFruOffset);
    }
}

TEST(FindFRUHeaderTest, TyanInvalidHeader)
{
    std::vector<uint8_t> data = {'$', 'T', 'Y', 'A', 'N', '$', 0, 0};
    data.resize(0x6000 + I2C_SMBUS_BLOCK_MAX);
    off_t offset = 0;
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);
    auto sections = findFRUHeader(reader, "error", offset);
    EXPECT_EQ(sections, std::nullopt);
}

TEST(FindFRUHeaderTest, TyanNoData)
{
    const std::vector<uint8_t> data = {'$', 'T', 'Y', 'A', 'N', '$', 0, 0};
    off_t offset = 0;
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);
    auto sections = findFRUHeader(reader, "error", offset);
    EXPECT_EQ(sections, std::nullopt);
}

TEST(FindFRUHeaderTest, TyanValidHeader)
{
    std::vector<uint8_t> data = {'$', 'T', 'Y', 'A', 'N', '$', 0, 0};
    data.resize(0x6000);
    constexpr std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> fruHeader = {
        0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0xf5};
    copy(fruHeader.begin(), fruHeader.end(), back_inserter(data));

    off_t offset = 0;
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);

    auto sections = findFRUHeader(reader, "error", offset);

    ASSERT_NE(sections, std::nullopt);
    if (sections)
    {
        EXPECT_EQ(0x6000, sections->IpmiFruOffset);
    }
}

TEST(FindFRUHeaderTest, GigaInvalidHeader)
{
    std::vector<uint8_t> data = {'G', 'I', 'G', 'A', 'B', 'Y', 'T', 'E'};
    data.resize(0x6000 + I2C_SMBUS_BLOCK_MAX);
    off_t offset = 0;
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);
    auto sections = findFRUHeader(reader, "error", offset);
    EXPECT_EQ(sections, std::nullopt);
}

TEST(FindFRUHeaderTest, GigaNoData)
{
    const std::vector<uint8_t> data = {'G', 'I', 'G', 'A', 'B', 'Y', 'T', 'E'};
    off_t offset = 0;
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);
    auto sections = findFRUHeader(reader, "error", offset);
    EXPECT_EQ(sections, std::nullopt);
}

TEST(FindFRUHeaderTest, GigaValidHeader)
{
    std::vector<uint8_t> data = {'G', 'I', 'G', 'A', 'B', 'Y', 'T', 'E'};
    data.resize(0x4000);
    constexpr std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> fruHeader = {
        0x01, 0x00, 0x01, 0x02, 0x03, 0x04, 0x00, 0xf5};
    copy(fruHeader.begin(), fruHeader.end(), back_inserter(data));

    off_t offset = 0;
    auto getData = [&data](auto o, std::span<uint8_t> b) {
        return getDataTempl(data, o, b);
    };
    FRUReader reader(getData);

    auto sections = findFRUHeader(reader, "error", offset);

    ASSERT_NE(sections, std::nullopt);
    if (sections)
    {
        EXPECT_EQ(0x4000, sections->IpmiFruOffset);
        EXPECT_EQ(512, sections->GigabyteXmlOffset);
    }
}

TEST(formatIPMIFRU, FullDecode)
{
    const std::array<uint8_t, 176> bmcFru = {
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
    std::flat_map<std::string, std::string, std::less<>> result;
    ASSERT_EQ(formatIPMIFRU(bmcFru, result), resCodes::resOK);

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

// Test for the `isFieldEditable` function
TEST(IsFieldEditableTest, ValidField)
{
    // Test with a valid field name that is editable
    // All PRODUCT fields are editable
    EXPECT_TRUE(isFieldEditable("PRODUCT_MANUFACTURER"));
    EXPECT_TRUE(isFieldEditable("PRODUCT_PART_NUMBER"));
    EXPECT_TRUE(isFieldEditable("PRODUCT_SERIAL_NUMBER"));
    EXPECT_TRUE(isFieldEditable("PRODUCT_VERSION"));
    EXPECT_TRUE(isFieldEditable("PRODUCT_PART_NUMBER"));
    EXPECT_TRUE(isFieldEditable("PRODUCT_PRODUCT_NAME"));
    EXPECT_TRUE(isFieldEditable("PRODUCT_ASSET_TAG"));
    EXPECT_TRUE(isFieldEditable("PRODUCT_FRU_VERSION_ID"));
    EXPECT_TRUE(isFieldEditable("PRODUCT_INFO_AM0"));

    // All BOARD fields are editable
    EXPECT_TRUE(isFieldEditable("BOARD_MANUFACTURER"));
    EXPECT_TRUE(isFieldEditable("BOARD_PART_NUMBER"));
    EXPECT_TRUE(isFieldEditable("BOARD_SERIAL_NUMBER"));
    EXPECT_TRUE(isFieldEditable("BOARD_FRU_VERSION_ID"));
    EXPECT_TRUE(isFieldEditable("BOARD_PRODUCT_NAME"));
    EXPECT_TRUE(isFieldEditable("BOARD_INFO_AM0"));
    EXPECT_TRUE(isFieldEditable("BOARD_INFO_AM10"));

    // All CHASSIS fields are editable
    EXPECT_TRUE(isFieldEditable("CHASSIS_PART_NUMBER"));
    EXPECT_TRUE(isFieldEditable("CHASSIS_SERIAL_NUMBER"));
    EXPECT_TRUE(isFieldEditable("CHASSIS_INFO_AM0"));
    EXPECT_TRUE(isFieldEditable("CHASSIS_INFO_AM10"));
}

TEST(IsFieldEditableTest, InvalidField)
{
    // Test with an invalid field name that is not editable
    EXPECT_FALSE(isFieldEditable("INVALID_FIELD"));
    EXPECT_FALSE(isFieldEditable("PRODUCT_INVALID_FIELD"));
    EXPECT_FALSE(isFieldEditable("BOARD_INVALID_FIELD"));
    EXPECT_FALSE(isFieldEditable("CHASSIS_INVALID_FIELD"));

    // Test with a field that does not match the expected pattern
    EXPECT_FALSE(isFieldEditable("PRODUCT_12345"));
    EXPECT_FALSE(isFieldEditable("BOARD_67890"));
    EXPECT_FALSE(isFieldEditable("ABCD_CHASSIS"));
    EXPECT_FALSE(isFieldEditable("ABCD_PRODUCT"));
    EXPECT_FALSE(isFieldEditable("ABCD_BOARD"));
}

TEST(UpdateAreaChecksumTest, EmptyArea)
{
    // Validates that an empty area does not cause any issues.
    std::vector<uint8_t> fruArea = {};
    EXPECT_FALSE(updateAreaChecksum(fruArea));
}

TEST(UpdateAreaChecksumTest, ValidArea)
{
    // Validates that a valid area updates the checksum correctly.
    std::vector<uint8_t> fruArea = {0x01, 0x00, 0x01, 0x02,
                                    0x03, 0x04, 0x00, 0x00};
    EXPECT_TRUE(updateAreaChecksum(fruArea));
    EXPECT_EQ(fruArea.back(), 0xf5);
}

TEST(UpdateAreaChecksumTest, InvalidArea)
{
    // Validates that an invalid area does not update the checksum.
    std::vector<uint8_t> fruArea = {0x01, 0x00, 0x01, 0x02, 0x03,
                                    0x04, 0x00, 0x00, 0xAA};
    EXPECT_FALSE(updateAreaChecksum(fruArea));
}

TEST(DisassembleFruDataTest, EmptyData)
{
    // Validates that an empty data vector returns false.
    std::vector<uint8_t> fruData = {};
    std::vector<std::vector<uint8_t>> areasData;
    EXPECT_FALSE(disassembleFruData(fruData, areasData));
}

TEST(DisassembleFruDataTest, ValidData)
{
    // Taken from qemu fby35_bmc_fruid
    std::vector<uint8_t> fruData = {
        0x01, 0x00, 0x00, 0x01, 0x0d, 0x00, 0x00, 0xf1, 0x01, 0x0c, 0x00, 0x36,
        0xe6, 0xd0, 0xc6, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0xd2, 0x42, 0x4d,
        0x43, 0x20, 0x53, 0x74, 0x6f, 0x72, 0x61, 0x67, 0x65, 0x20, 0x4d, 0x6f,
        0x64, 0x75, 0x6c, 0x65, 0xcd, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58,
        0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0xce, 0x58, 0x58, 0x58, 0x58, 0x58,
        0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0xc3, 0x31, 0x2e,
        0x30, 0xc9, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0xd2,
        0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58,
        0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0xc1, 0x39, 0x01, 0x0c, 0x00, 0xc6,
        0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0xd2, 0x59, 0x6f, 0x73, 0x65, 0x6d,
        0x69, 0x74, 0x65, 0x20, 0x56, 0x33, 0x2e, 0x35, 0x20, 0x45, 0x56, 0x54,
        0x32, 0xce, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58,
        0x58, 0x58, 0x58, 0x58, 0xc4, 0x45, 0x56, 0x54, 0x32, 0xcd, 0x58, 0x58,
        0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0xc7,
        0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0xc3, 0x31, 0x2e, 0x30, 0xc9,
        0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0x58, 0xc8, 0x43, 0x6f,
        0x6e, 0x66, 0x69, 0x67, 0x20, 0x41, 0xc1, 0x45,
    };

    std::vector<std::vector<uint8_t>> areasData;
    ASSERT_TRUE(disassembleFruData(fruData, areasData));
    EXPECT_GT(areasData.size(), 1U);

    // Internal area is size is zero
    EXPECT_EQ(areasData[static_cast<size_t>(fruAreas::fruAreaInternal)].size(),
              0U);
    // Chassis are is zero
    EXPECT_EQ(areasData[static_cast<size_t>(fruAreas::fruAreaChassis)].size(),
              0U);
    // Board area is 96 byte
    EXPECT_EQ(areasData[static_cast<size_t>(fruAreas::fruAreaBoard)].size(),
              96U);
    // Product area is 96 byte
    EXPECT_EQ(areasData[static_cast<size_t>(fruAreas::fruAreaProduct)].size(),
              96U);

    // Multi-record area is 64 byte.
    EXPECT_EQ(
        areasData[static_cast<size_t>(fruAreas::fruAreaMultirecord)].size(),
        0U);

    EXPECT_TRUE(setField(fruAreas::fruAreaBoard,
                         areasData[static_cast<size_t>(fruAreas::fruAreaBoard)],
                         "BOARD_INFO_AM1", "01"));
    EXPECT_TRUE(setField(fruAreas::fruAreaBoard,
                         areasData[static_cast<size_t>(fruAreas::fruAreaBoard)],
                         "BOARD_INFO_AM2", "MAC: 3C:6D:66:14:C8:7A"));
    // set Product fields
    EXPECT_TRUE(
        setField(fruAreas::fruAreaProduct,
                 areasData[static_cast<size_t>(fruAreas::fruAreaProduct)],
                 "PRODUCT_ASSET_TAG", "123"));
    EXPECT_TRUE(
        setField(fruAreas::fruAreaProduct,
                 areasData[static_cast<size_t>(fruAreas::fruAreaProduct)],
                 "PRODUCT_PART_NUMBER", "699-13809-0404-600"));
    EXPECT_TRUE(
        setField(fruAreas::fruAreaProduct,
                 areasData[static_cast<size_t>(fruAreas::fruAreaProduct)],
                 "PRODUCT_PRODUCT_NAME", "OpenBMC-test1"));

    EXPECT_EQ(
        areasData[static_cast<size_t>(fruAreas::fruAreaProduct)].size() % 8, 0);
    EXPECT_EQ(areasData[static_cast<size_t>(fruAreas::fruAreaBoard)].size() % 8,
              0);

    std::vector<uint8_t> assembledData;
    EXPECT_TRUE(assembleFruData(assembledData, areasData));

    std::flat_map<std::string, std::string, std::less<>> result;
    auto rescode = formatIPMIFRU(assembledData, result);
    EXPECT_NE(rescode, resCodes::resErr);

    EXPECT_EQ(result["PRODUCT_ASSET_TAG"], "123");
    EXPECT_EQ(result["PRODUCT_PART_NUMBER"], "699-13809-0404-600");
    EXPECT_EQ(result["PRODUCT_PRODUCT_NAME"], "OpenBMC-test1");
    EXPECT_EQ(result["BOARD_INFO_AM1"], "01");
    EXPECT_EQ(result["BOARD_INFO_AM2"], "MAC: 3C:6D:66:14:C8:7A");
}

TEST(ReassembleFruDataTest, UnalignedFails)
{
    std::vector<uint8_t> areaOne{0, 35};
    std::vector<uint8_t> areaTwo{0, 32};
    std::vector<std::vector<uint8_t>> areas;
    areas.push_back(areaOne);
    areas.push_back(areaTwo);

    std::vector<uint8_t> fruData;
    EXPECT_FALSE(assembleFruData(fruData, areas));
}

constexpr auto gzip = std::to_array<uint8_t>(
    {0x1f, 0x8b, 0x08, 0x08, 0x74, 0x47, 0xe4, 0x68, 0x00, 0x03, 0x66, 0x72,
     0x75, 0x2e, 0x62, 0x69, 0x6e, 0x00, 0x9d, 0x91, 0xdf, 0x0a, 0x82, 0x30,
     0x18, 0xc5, 0xef, 0x7d, 0x8a, 0xe1, 0x7d, 0x4d, 0xad, 0x0b, 0x1b, 0x73,
     0x52, 0x9a, 0x21, 0x51, 0x37, 0xcb, 0x07, 0x18, 0x6e, 0xea, 0xa0, 0x36,
     0x58, 0x12, 0x3d, 0x7e, 0x29, 0x5a, 0x5a, 0x08, 0xd1, 0xdd, 0x7e, 0xdf,
     0x9f, 0x9d, 0xc3, 0xf9, 0x70, 0x78, 0xbf, 0x9c, 0xc1, 0x4d, 0x98, 0xab,
     0xd4, 0x2a, 0xb0, 0xdd, 0xb9, 0x63, 0x03, 0xa1, 0x72, 0xcd, 0xa5, 0x2a,
     0x03, 0x3b, 0x3b, 0x25, 0x33, 0xdf, 0x0e, 0x89, 0x85, 0x77, 0x94, 0xee,
     0x33, 0x62, 0x01, 0x00, 0xf0, 0x46, 0x33, 0xc3, 0x53, 0x55, 0xe8, 0x16,
     0x9b, 0xca, 0x81, 0x49, 0xd5, 0x43, 0xc3, 0xc7, 0x34, 0x1a, 0x60, 0x53,
     0x49, 0x55, 0x2d, 0x4c, 0xc1, 0x72, 0xe1, 0x8e, 0x1b, 0xed, 0xb6, 0xe6,
     0x82, 0xc4, 0x82, 0xcb, 0x9c, 0xd5, 0x82, 0x63, 0xd8, 0xf2, 0xf7, 0x14,
     0xcb, 0xd7, 0x9c, 0x1b, 0x87, 0xb8, 0x0e, 0x4a, 0x12, 0xb4, 0x75, 0xd0,
     0x62, 0x85, 0xfc, 0x25, 0x8a, 0xa3, 0xe7, 0x46, 0xdf, 0x1b, 0x8b, 0xc2,
     0x29, 0xd5, 0xb7, 0x1d, 0x6f, 0xc2, 0x0e, 0xad, 0x98, 0xf9, 0xc3, 0x4b,
     0xfc, 0x83, 0x97, 0x0f, 0x49, 0x4c, 0x6b, 0x23, 0x54, 0x59, 0x57, 0xc4,
     0xc3, 0xf0, 0xf5, 0x1e, 0x84, 0x09, 0x07, 0x69, 0x36, 0xdf, 0x77, 0x51,
     0x63, 0x38, 0xb8, 0x03, 0x86, 0xdd, 0x7d, 0x1e, 0x15, 0xc1, 0xa2, 0x29,
     0xcf, 0x01, 0x00, 0x00});

TEST(GzipUtils, parseMacFromGzipXmlHeader)
{
    FRUReader reader(std::bind_front(getDataTempl, gzip));

    std::string mac = parseMacFromGzipXmlHeader(reader, 0);
    EXPECT_EQ(mac, "10:FF:E0:39:84:DC");
}
