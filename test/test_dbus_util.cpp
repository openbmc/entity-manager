#include "dbus_util.hpp"

#include <gtest/gtest.h>

TEST(DBUS_UTIL, SanitizeIntfEmpty)
{
    const std::string intf;

    ASSERT_EQ(dbus_util::sanitizeForDBusInterface(intf), "_");
}

TEST(DBUS_UTIL, SanitizeIntfSegmentNotBeginWithDigit)
{
    const std::string intf = "9.Stepwise";

    ASSERT_EQ(dbus_util::sanitizeForDBusInterface(intf), "_.Stepwise");
}

TEST(DBUS_UTIL, SanitizeIntfSegmentNotBeginWithDigit2)
{
    const std::string intf = "PID.9";

    ASSERT_EQ(dbus_util::sanitizeForDBusInterface(intf), "PID._");
}

TEST(DBUS_UTIL, SanitizeIntfEmptySegment)
{
    const std::string intf = "..";

    ASSERT_EQ(dbus_util::sanitizeForDBusInterface(intf), "_._");
}

TEST(DBUS_UTIL, SanitizeIntfEmptySegmentEnd)
{
    const std::string intf = "Configuration.";

    ASSERT_EQ(dbus_util::sanitizeForDBusInterface(intf), "Configuration._");
}

TEST(DBUS_UTIL, SanitizeIntfNop)
{
    const std::string intf = "Tyan_S8030_Baseboard";

    ASSERT_EQ(dbus_util::sanitizeForDBusInterface(intf),
              "Tyan_S8030_Baseboard");
}

TEST(DBUS_UTIL, SanitizeIntfNop2)
{
    const std::string intf = "Pid.Zone";

    ASSERT_EQ(dbus_util::sanitizeForDBusInterface(intf), "Pid.Zone");
}

TEST(DBUS_UTIL, SanitizeIntfSimple)
{
    const std::string intf = "Tyan S8030 Baseboard";

    ASSERT_EQ(dbus_util::sanitizeForDBusInterface(intf),
              "Tyan_S8030_Baseboard");
}

TEST(DBUS_UTIL, SanitizeIntfWithDot)
{
    const std::string intf = "xyz.openbmc_project.Configuration.ADC";

    ASSERT_EQ(dbus_util::sanitizeForDBusInterface(intf),
              "xyz.openbmc_project.Configuration.ADC");
}

TEST(DBUS_UTIL, SanitizePathSegmentEmpty)
{
    const std::string path;

    ASSERT_EQ(dbus_util::sanitizeForDBusPathSegment(path), "_");
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
