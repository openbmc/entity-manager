#include "FruUtils.hpp"

#include <array>

#include "gtest/gtest.h"

extern "C"
{
// Include for I2C_SMBUS_BLOCK_MAX
#include <linux/i2c.h>
}

TEST(ValidateHeaderTest, InvalidFruVersionReturnsFalse)
{
    // Validates the FruVersion is checked for the only legal value.
    constexpr std::array<uint8_t, I2C_SMBUS_BLOCK_MAX> fru_header = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(validateHeader(fru_header));
}

TEST(VerifyOffsetTest, EmptyFruDataReturnsFalse)
{
    // Validates the FruData size is checked for non empty.
    std::vector<uint8_t> fru_data = {};

    EXPECT_FALSE(verifyOffset(fru_data, fruAreas::fruAreaChassis, 0));
}

TEST(VerifyOffsetTest, ValidInputDataReturnsTrue)
{
    // Validates all inputs with expected value.
    const std::vector<uint8_t> fru_data = {0x01, 0x00, 0x01, 0x02, 0x03,
                                           0x04, 0x00, 0x00, 0x00};

    EXPECT_TRUE(verifyOffset(fru_data, fruAreas::fruAreaChassis, 1));
}

TEST(VerifyOffsetTest, WrongAreaReturnsFalse)
{
    // Validates the FruArea value, check if it is within range.
    const std::vector<uint8_t> fru_data = {0x01, 0x00, 0x00, 0x00, 0x00,
                                           0x00, 0x00, 0x00, 0x00};

    EXPECT_FALSE(verifyOffset(fru_data, static_cast<fruAreas>(8), 0));
}

TEST(VerifyOffsetTest, OverlapAreaReturnsFalse)
{
    // Validates the Overlap of offsets with overlapped values.
    const std::vector<uint8_t> fru_data = {0x01, 0x00, 0x01, 0x02, 0x03,
                                           0x04, 0x00, 0x00, 0x00};

    EXPECT_FALSE(verifyOffset(fru_data, fruAreas::fruAreaChassis, 2));
}
