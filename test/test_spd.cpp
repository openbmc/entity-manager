#include "spd/spd.hpp"

#include "gtest/gtest.h"

TEST(ValidateSpdContentTest, SizeNotEnough)
{
    const std::vector<uint8_t> spdContent = {0x30, 0x09};
    EXPECT_EQ(Spd::getSpdType(spdContent), Spd::SpdType::kTypeUnknown);
    EXPECT_EQ(Spd::getSpdSize(spdContent), -1);
}

TEST(ValidateSpdContentTest, TypeUnsupported)
{
    // DDR1 is currently not supported.
    const uint8_t type = static_cast<uint8_t>(Spd::SpdType::kTypeDdr1);
    const std::vector<uint8_t> spdContent = {0x30, 0x09, type};
    EXPECT_EQ(Spd::getFromImage(spdContent), nullptr);
}

TEST(ValidateSpdContentTest, TypeUnrecognized)
{
    const uint8_t type = static_cast<uint8_t>(Spd::SpdType::kTypeUnknown);
    const std::vector<uint8_t> spdContent = {0x30, 0x09, type};
    EXPECT_EQ(Spd::getFromImage(spdContent), nullptr);
}

TEST(ValidateSpdContentTest, SizeUnsupported)
{
    const uint8_t size = 0x00;
    const std::vector<uint8_t> spdContent = {size, 0x09, 0x12};
    EXPECT_EQ(Spd::getSpdSize(spdContent), -1);
}

TEST(ValidateSpdContentTest, SizeSupported)
{
    const uint8_t size = 0x30;
    const std::vector<uint8_t> spdContent = {size, 0x09, 0x12};
    EXPECT_EQ(Spd::getSpdSize(spdContent), 1024);
}

TEST(ValidateCrcTest, CrcCorrect)
{
    const std::vector<uint8_t> spdContent = {
        0x30, 0x09, 0x12, 0x01, 0x04, 0x20, 0x00, 0x62, 0x00, 0x00, 0x00, 0x00,
        0x90, 0x02, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa0, 0x01, 0xf2, 0x03,
        0x7a, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x80, 0x3e, 0x80, 0x3e, 0x80, 0x3e,
        0x00, 0x7d, 0x80, 0xbb, 0x30, 0x75, 0x27, 0x01, 0xa0, 0x00, 0x82, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x09, 0x00, 0x80, 0xb3, 0x80, 0x21, 0x0b, 0x2a, 0x81, 0x10, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xb3, 0xc0, 0x12, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x11, 0x21, 0x01, 0x82, 0x08, 0x2a, 0x00, 0x00, 0x00, 0x00,
        0x86, 0x9d, 0x80, 0x13, 0x00, 0x00, 0x00, 0x00, 0x20, 0x08, 0x55, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(Spd::calJedecCrc16(spdContent), 0x4B5B);
}
