#include "devicetree_vpd_parser/machine_context.hpp"

#include <sdbusplus/server/manager.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <gtest/gtest.h>

class DeviceTreeVpdTest : public ::testing::Test
{
  protected:
    std::filesystem::path dir;

    void SetUp() override
    {
        std::string pattern =
            (std::filesystem::temp_directory_path() / "entity-vpd-XXXXXX")
                .string();
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        char* path = mkdtemp(buffer.data());
        ASSERT_NE(path, nullptr);
        dir = path;
    }

    void TearDown() override
    {
        if (!dir.empty())
        {
            std::error_code error;
            std::filesystem::remove_all(dir, error);
            EXPECT_FALSE(error) << error.message();
        }
    }

    void writeNode(const std::string& name, const std::string& value)
    {
        std::ofstream file(dir / name);
        ASSERT_TRUE(file);
        file << value;
        file.close();
        ASSERT_TRUE(file);
    }
};

TEST_F(DeviceTreeVpdTest, EmptyNodeDoesNotBlockFollowingProperties)
{
    writeNode("model", "");
    writeNode("serial-number", "serial-123");
    writeNode("part-number", "part-456");
    writeNode("manufacturer", "manufacturer");
    ASSERT_FALSE(HasFatalFailure());

    sdbusplus::async::context ctx{sdbusplus::bus::new_default_user()};
    sdbusplus::server::manager_t manager{ctx, "/xyz/openbmc_project"};
    MachineContext machine{ctx, "/xyz/openbmc_project/MachineContext", dir};

    EXPECT_EQ(machine.model(), "");
    EXPECT_EQ(machine.serial_number(), "serial-123");
    EXPECT_EQ(machine.part_number(), "part-456");
    EXPECT_EQ(machine.manufacturer(), "manufacturer");
}

TEST_F(DeviceTreeVpdTest, MissingNodeDoesNotBlockFollowingProperties)
{
    writeNode("model", "model-123");
    writeNode("part-number", "part-456");
    writeNode("manufacturer", "manufacturer");
    ASSERT_FALSE(HasFatalFailure());

    sdbusplus::async::context ctx{sdbusplus::bus::new_default_user()};
    sdbusplus::server::manager_t manager{ctx, "/xyz/openbmc_project"};
    MachineContext machine{ctx, "/xyz/openbmc_project/MachineContext", dir};

    EXPECT_EQ(machine.model(), "model-123");
    EXPECT_EQ(machine.serial_number(), "");
    EXPECT_EQ(machine.part_number(), "part-456");
    EXPECT_EQ(machine.manufacturer(), "manufacturer");
}

TEST_F(DeviceTreeVpdTest, OpenErrorDoesNotBlockFollowingProperties)
{
    writeNode("model", "model-123");
    writeNode("not-a-directory", "value");
    writeNode("part-number", "part-456");
    ASSERT_FALSE(HasFatalFailure());
    std::filesystem::create_symlink("not-a-directory/child",
                                    dir / "serial-number");

    sdbusplus::async::context ctx{sdbusplus::bus::new_default_user()};
    sdbusplus::server::manager_t manager{ctx, "/xyz/openbmc_project"};
    MachineContext machine{ctx, "/xyz/openbmc_project/MachineContext", dir};

    EXPECT_EQ(machine.model(), "model-123");
    EXPECT_EQ(machine.serial_number(), "");
    EXPECT_EQ(machine.part_number(), "part-456");
}
