#include "fru_device/inotify.hpp"

#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

#include <gtest/gtest.h>

TEST(InotifyTest, CreatesValidDescriptor)
{
    auto fd = createInotify();
    if (!fd)
    {
        FAIL() << "Unable to initialize inotify";
        return;
    }
    EXPECT_GE(fcntl(*fd, F_GETFD), 0);
    EXPECT_EQ(close(*fd), 0);
}

TEST(InotifyTest, ReturnsEmptyWhenDescriptorLimitIsReached)
{
    ASSERT_EXIT(
        {
            const rlimit limit{};
            if (setrlimit(RLIMIT_NOFILE, &limit) != 0)
            {
                _exit(2);
            }
            _exit(createInotify().has_value() ? 1 : 0);
        },
        ::testing::ExitedWithCode(0), "");
}
