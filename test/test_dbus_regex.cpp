#include "dbus_regex.hpp"

#include <gtest/gtest.h>

TEST(DBUS_REGEX, SanitizePathNop)
{
    std::string path = "Tyan_S8030_Baseboard";
    dbus_regex::sanitizePath(path);

    ASSERT_EQ(path, "Tyan_S8030_Baseboard");
}

TEST(DBUS_REGEX, SanitizePathSimple)
{
    std::string path = "Tyan S8030 Baseboard";
    dbus_regex::sanitizePath(path);

    ASSERT_EQ(path, "Tyan_S8030_Baseboard");
}

TEST(DBUS_REGEX, SanitizePathWithDot)
{
    std::string path = "xyz.openbmc_project.Configuration.ADC";
    dbus_regex::sanitizePath(path);

    ASSERT_EQ(path, "xyz.openbmc_project.Configuration.ADC");
}

TEST(DBUS_REGEX, SanitizeMemberNop)
{
    std::string path = "P0_VDD_MEM_ABCD";
    dbus_regex::sanitizeMember(path);

    ASSERT_EQ(path, "P0_VDD_MEM_ABCD");
}

TEST(DBUS_REGEX, SanitizeMember)
{
    std::string path = "P0 VDD bus % 13";
    dbus_regex::sanitizeMember(path);

    ASSERT_EQ(path, "P0_VDD_bus___13");
}

TEST(DBUS_REGEX, SanitizeMemberWithDot)
{
    std::string path = "xyz.openbmc_project.Configuration.ADC";
    dbus_regex::sanitizeMember(path);

    ASSERT_EQ(path, "xyz_openbmc_project_Configuration_ADC");
}
