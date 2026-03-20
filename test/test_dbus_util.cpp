#include "dbus_util.hpp"

#include <gtest/gtest.h>

TEST(DBUS_UTIL, SanitizePathNop)
{
    std::string path = "Tyan_S8030_Baseboard";
    path = dbus_util::sanitizeForDBusPath(path);

    ASSERT_EQ(path, "Tyan_S8030_Baseboard");
}

TEST(DBUS_UTIL, SanitizePathSimple)
{
    std::string path = "Tyan S8030 Baseboard";
    path = dbus_util::sanitizeForDBusPath(path);

    ASSERT_EQ(path, "Tyan_S8030_Baseboard");
}

TEST(DBUS_UTIL, SanitizePathWithDot)
{
    std::string path = "xyz.openbmc_project.Configuration.ADC";
    path = dbus_util::sanitizeForDBusPath(path);

    ASSERT_EQ(path, "xyz.openbmc_project.Configuration.ADC");
}

TEST(DBUS_UTIL, SanitizeMemberNop)
{
    std::string path = "P0_VDD_MEM_ABCD";
    path = dbus_util::sanitizeForDBusMember(path);

    ASSERT_EQ(path, "P0_VDD_MEM_ABCD");
}

TEST(DBUS_UTIL, SanitizeMember)
{
    std::string path = "P0 VDD bus % 13";
    path = dbus_util::sanitizeForDBusMember(path);

    ASSERT_EQ(path, "P0_VDD_bus___13");
}

TEST(DBUS_UTIL, SanitizeMemberWithDot)
{
    std::string path = "xyz.openbmc_project.Configuration.ADC";
    path = dbus_util::sanitizeForDBusMember(path);

    ASSERT_EQ(path, "xyz_openbmc_project_Configuration_ADC");
}
