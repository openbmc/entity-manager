#include "dbus_util.hpp"

#include <gtest/gtest.h>

TEST(DBUS_UTIL, ValidateIntfEmpty)
{
    const std::string intf;

    ASSERT_FALSE(dbus_util::validateDBusInterfaceSegments(intf));
}

TEST(DBUS_UTIL, ValidateIntfSegmentNotBeginWithDigit)
{
    const std::string intf = "9.Stepwise";

    ASSERT_FALSE(dbus_util::validateDBusInterfaceSegments(intf));
}

TEST(DBUS_UTIL, ValidateIntfSegmentNotBeginWithDigit2)
{
    const std::string intf = "PID.9";

    ASSERT_FALSE(dbus_util::validateDBusInterfaceSegments(intf));
}

TEST(DBUS_UTIL, ValidateIntfEmptySegment)
{
    const std::string intf = "..";

    ASSERT_FALSE(dbus_util::validateDBusInterfaceSegments(intf));
}

TEST(DBUS_UTIL, ValidateIntfEmptySegmentEnd)
{
    const std::string intf = "Configuration.";

    ASSERT_FALSE(dbus_util::validateDBusInterfaceSegments(intf));
}

TEST(DBUS_UTIL, ValidateIntfNop)
{
    const std::string intf = "Tyan_S8030_Baseboard";

    ASSERT_TRUE(dbus_util::validateDBusInterfaceSegments(intf));
}

TEST(DBUS_UTIL, ValidateIntfUnderscore)
{
    const std::string intf = "_Intf";

    ASSERT_TRUE(dbus_util::validateDBusInterfaceSegments(intf));
}

TEST(DBUS_UTIL, ValidateIntfNop2)
{
    const std::string intf = "Pid.Zone";

    ASSERT_TRUE(dbus_util::validateDBusInterfaceSegments(intf));
}

TEST(DBUS_UTIL, ValidateIntfSimple)
{
    const std::string intf = "Tyan S8030 Baseboard";

    ASSERT_FALSE(dbus_util::validateDBusInterfaceSegments(intf));
}

TEST(DBUS_UTIL, ValidateIntfWithDot)
{
    const std::string intf = "xyz.openbmc_project.Configuration.ADC";

    ASSERT_TRUE(dbus_util::validateDBusInterfaceSegments(intf));
}

TEST(DBUS_UTIL, SanitizePathSegmentNop)
{
    const std::string path = "P0_VDD_MEM_ABCD";

    ASSERT_EQ(dbus_util::sanitizeForDBusPathSegment(path), "P0_VDD_MEM_ABCD");
}

TEST(DBUS_UTIL, SanitizePathSegment)
{
    const std::string path = "P0 VDD bus % 13";

    ASSERT_EQ(dbus_util::sanitizeForDBusPathSegment(path), "P0_VDD_bus___13");
}

TEST(DBUS_UTIL, SanitizePathSegmentWithDot)
{
    const std::string path = "xyz.openbmc_project.Configuration.ADC";

    ASSERT_EQ(dbus_util::sanitizeForDBusPathSegment(path),
              "xyz_openbmc_project_Configuration_ADC");
}
