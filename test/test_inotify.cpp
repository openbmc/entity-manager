#include "fru_device/inotify.hpp"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

class InotifyTest : public ::testing::Test
{
  public:
    static inline bool failInit = false;
    static inline bool failWatch = false;
    static inline int lastDescriptor = -1;

  protected:
    void SetUp() override
    {
        failInit = false;
        failWatch = false;
        lastDescriptor = -1;
        std::string pattern =
            (std::filesystem::temp_directory_path() / "inotify-test-XXXXXX")
                .string();
        char* directory = mkdtemp(pattern.data());
        ASSERT_NE(directory, nullptr);
        path = directory;
    }

    void TearDown() override
    {
        failInit = false;
        failWatch = false;
        if (lastDescriptor >= 0 && fcntl(lastDescriptor, F_GETFD) >= 0)
        {
            EXPECT_EQ(close(lastDescriptor), 0);
        }
        if (!path.empty())
        {
            std::filesystem::remove_all(path);
        }
    }

    std::filesystem::path path;
};

extern "C" int realInotifyInit() asm("__real_inotify_init");
extern "C" int wrapInotifyInit() asm("__wrap_inotify_init");
extern "C" int realInotifyAddWatch(int fd, const char* path, uint32_t mask) asm(
    "__real_inotify_add_watch");
extern "C" int wrapInotifyAddWatch(int fd, const char* path, uint32_t mask) asm(
    "__wrap_inotify_add_watch");

extern "C" int wrapInotifyInit()
{
    if (InotifyTest::failInit)
    {
        errno = EMFILE;
        return -1;
    }

    InotifyTest::lastDescriptor = realInotifyInit();
    return InotifyTest::lastDescriptor;
}

extern "C" int wrapInotifyAddWatch(int fd, const char* path, uint32_t mask)
{
    if (InotifyTest::failWatch)
    {
        errno = ENOSPC;
        return -1;
    }

    return realInotifyAddWatch(fd, path, mask);
}

TEST_F(InotifyTest, CreatesValidDescriptor)
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

TEST_F(InotifyTest, ReturnsEmptyWhenDescriptorLimitIsReached)
{
    failInit = true;
    auto fd = createInotifyWatch(path.c_str(), IN_CREATE);
    failInit = false;
    EXPECT_FALSE(fd.has_value());
}

TEST_F(InotifyTest, WatchReceivesDirectoryEvents)
{
    auto fd =
        createInotifyWatch(path.c_str(), IN_CREATE | IN_MOVED_TO | IN_DELETE);
    if (!fd)
    {
        FAIL() << "Unable to create directory watch";
        return;
    }

    const auto file = path / "i2c-test";
    int created = open(file.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
    ASSERT_GE(created, 0);
    ASSERT_EQ(close(created), 0);

    pollfd eventFd{*fd, POLLIN, 0};
    ASSERT_EQ(poll(&eventFd, 1, 1000), 1);
    ASSERT_NE(eventFd.revents & POLLIN, 0);
    std::array<char, 4096> buffer{};
    ASSERT_GE(read(*fd, buffer.data(), buffer.size()),
              static_cast<ssize_t>(sizeof(inotify_event)));
    inotify_event event{};
    std::memcpy(&event, buffer.data(), sizeof(event));
    EXPECT_NE(event.mask & IN_CREATE, 0);
    EXPECT_EQ(close(*fd), 0);
}

TEST_F(InotifyTest, MissingPathClosesDescriptor)
{
    const auto missing = path / "missing";
    auto fd = createInotifyWatch(missing.c_str(), IN_CREATE);
    EXPECT_FALSE(fd.has_value());
    ASSERT_GE(lastDescriptor, 0);
    errno = 0;
    EXPECT_EQ(fcntl(lastDescriptor, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);
}

TEST_F(InotifyTest, WatchLimitFailureClosesDescriptor)
{
    failWatch = true;
    auto fd = createInotifyWatch(path.c_str(), IN_CREATE);
    failWatch = false;
    EXPECT_FALSE(fd.has_value());
    ASSERT_GE(lastDescriptor, 0);
    errno = 0;
    EXPECT_EQ(fcntl(lastDescriptor, F_GETFD), -1);
    EXPECT_EQ(errno, EBADF);
}
