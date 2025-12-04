#include "dbus_regex.hpp"

#include <gtest/gtest.h>

TEST(DBUS_REGEX, SanitizePathNop)
{
    std::string path = "Tyan_S8030_Baseboard";
    path = dbus_regex::sanitizeForDBusPath(path);

    ASSERT_EQ(path, "Tyan_S8030_Baseboard");
}

TEST(DBUS_REGEX, SanitizePathSimple)
{
    std::string path = "Tyan S8030 Baseboard";
    path = dbus_regex::sanitizeForDBusPath(path);

    ASSERT_EQ(path, "Tyan_S8030_Baseboard");
}

TEST(DBUS_REGEX, SanitizePathWithDot)
{
    std::string path = "xyz.openbmc_project.Configuration.ADC";
    path = dbus_regex::sanitizeForDBusPath(path);

    ASSERT_EQ(path, "xyz.openbmc_project.Configuration.ADC");
}

TEST(DBUS_REGEX, SanitizeMemberNop)
{
    std::string path = "P0_VDD_MEM_ABCD";
    path = dbus_regex::sanitizeForDBusMember(path);

    ASSERT_EQ(path, "P0_VDD_MEM_ABCD");
}

TEST(DBUS_REGEX, SanitizeMember)
{
    std::string path = "P0 VDD bus % 13";
    path = dbus_regex::sanitizeForDBusMember(path);

    ASSERT_EQ(path, "P0_VDD_bus___13");
}

TEST(DBUS_REGEX, SanitizeMemberWithDot)
{
    std::string path = "xyz.openbmc_project.Configuration.ADC";
    path = dbus_regex::sanitizeForDBusMember(path);

    ASSERT_EQ(path, "xyz_openbmc_project_Configuration_ADC");
}
