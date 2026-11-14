#include "fru_device/inotify.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>

#include <gtest/gtest.h>

namespace
{
bool failInotifyInit = false;
}

extern "C" int __real_inotify_init();

extern "C" int __wrap_inotify_init()
{
    if (failInotifyInit)
    {
        errno = EMFILE;
        return -1;
    }

    return __real_inotify_init();
}

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
    failInotifyInit = true;
    auto fd = createInotify();
    failInotifyInit = false;
    EXPECT_FALSE(fd.has_value());
}
