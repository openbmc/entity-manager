#include "utils.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
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

constexpr std::string_view helloWorld = "Hello World";

TEST(IfindFirstTest, BasicMatch)
{
    auto match = iFindFirst(helloWorld, "World");
    EXPECT_TRUE(match);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.begin()), 6);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.end()), 11);
}

TEST(IfindFirstTest, CaseInsensitiveMatch)
{
    auto match = iFindFirst(helloWorld, "world");
    EXPECT_TRUE(match);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.begin()), 6);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.end()), 11);
}

TEST(IfindFirstTest, NoMatch)
{
    auto match = iFindFirst(helloWorld, "Planet");
    EXPECT_FALSE(match);
}

TEST(IfindFirstTest, MatchAtStart)
{
    auto match = iFindFirst(helloWorld, "HeLLo");
    EXPECT_TRUE(match);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.begin()), 0);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.end()), 5);
}

TEST(IfindFirstTest, MatchAtEnd)
{
    auto match = iFindFirst(helloWorld, "LD");
    EXPECT_TRUE(match);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.begin()), 9);
    EXPECT_EQ(std::distance(helloWorld.begin(), match.end()), 11);
}

TEST(IfindFirstTest, EmptySubstring)
{
    auto match = iFindFirst(helloWorld, "");
    EXPECT_FALSE(match);
}

TEST(IfindFirstTest, EmptyString)
{
    auto match = iFindFirst("", "Hello");
    EXPECT_FALSE(match);
}

TEST(SplitTest, NormalSplit)
{
    auto result = split("a,b,c", ',');
    std::vector<std::string> expected = {"a", "b", "c"};
    EXPECT_EQ(result, expected);
}

TEST(SplitTest, ConsecutiveDelimiters)
{
    auto result = split("a,,b", ',');
    std::vector<std::string> expected = {"a", "", "b"};
    EXPECT_EQ(result, expected);
}

TEST(SplitTest, LeadingDelimiter)
{
    auto result = split(",a,b", ',');
    std::vector<std::string> expected = {"", "a", "b"};
    EXPECT_EQ(result, expected);
}

TEST(SplitTest, TrailingDelimiter)
{
    auto result = split("a,b,", ',');
    std::vector<std::string> expected = {"a", "b", ""};
    EXPECT_EQ(result, expected);
}

TEST(SplitTest, NoDelimiter)
{
    auto result = split("abc", ',');
    std::vector<std::string> expected = {"abc"};
    EXPECT_EQ(result, expected);
}

TEST(SplitTest, EmptyString)
{
    auto result = split("", ',');
    std::vector<std::string> expected = {""};
    EXPECT_EQ(result, expected);
}

TEST(ReplaceAllTest, BasicReplacement)
{
    std::string str = "hello world, world!";
    replaceAll(str, "world", "earth");
    EXPECT_EQ(str, "hello earth, earth!");
}

TEST(ReplaceAllTest, NoMatch)
{
    std::string str = "hello world";
    replaceAll(str, "xxx", "abc");
    EXPECT_EQ(str, "hello world");
}

TEST(ReplaceAllTest, ReplaceWithEmpty)
{
    std::string str = "apple apple";
    replaceAll(str, "apple", "");
    EXPECT_EQ(str, " ");
}

TEST(ReplaceAllTest, ReplaceEmptySearch)
{
    std::string str = "abc";
    replaceAll(str, "", "x");
    EXPECT_EQ(str, "abc");
}

TEST(IReplaceAllTest, CaseInsensitive)
{
    std::string str = "Hello hEllo heLLo";
    iReplaceAll(str, "hello", "hi");
    EXPECT_EQ(str, "hi hi hi");
}

TEST(IReplaceAllTest, MixedContent)
{
    std::string str = "Hello World! WORLD world";
    iReplaceAll(str, "world", "Earth");
    EXPECT_EQ(str, "Hello Earth! Earth Earth");
}

TEST(IReplaceAllTest, NoMatchCaseInsensitive)
{
    std::string str = "Good Morning";
    iReplaceAll(str, "night", "day");
    EXPECT_EQ(str, "Good Morning");
}

TEST(IReplaceAllTest, ReplaceWithEmptyCaseInsensitive)
{
    std::string str = "ABC abc AbC";
    iReplaceAll(str, "abc", "");
    EXPECT_EQ(str, "  ");
}

TEST(IReplaceAllTest, ReplacementContainsSearch)
{
    std::string str = "a A";
    iReplaceAll(str, "a", "aa");
    EXPECT_EQ(str, "aa aa");
}

TEST(IReplaceAllTest, ReplacementContainsSearchWithDifferentCase)
{
    std::string str = "abc ABC";
    iReplaceAll(str, "abc", "xAbCy");
    EXPECT_EQ(str, "xAbCy xAbCy");
}

TEST(ToLowerCopyTest, BasicTests)
{
    EXPECT_EQ(toLowerCopy("HelloWorld"), "helloworld");

    EXPECT_EQ(toLowerCopy("HELLOWORLD"), "helloworld");

    EXPECT_EQ(toLowerCopy("helloworld"), "helloworld");

    EXPECT_EQ(toLowerCopy("123ABC!@#"), "123abc!@#");

    EXPECT_EQ(toLowerCopy("!@#$%^&*()_+"), "!@#$%^&*()_+");

    EXPECT_EQ(toLowerCopy(""), "");
}

TEST(ParseMuxDeviceRootBusTest, BasicTests)
{
    EXPECT_EQ(parseMuxDeviceRootBus("7-0070"), 7);
    EXPECT_EQ(parseMuxDeviceRootBus("77-0070"), 77);
    EXPECT_EQ(parseMuxDeviceRootBus("0-0070"), 0);
    EXPECT_EQ(parseMuxDeviceRootBus("i2c-mux-gpio"), -1);
    EXPECT_EQ(parseMuxDeviceRootBus("-0070"), -1);
    EXPECT_EQ(parseMuxDeviceRootBus("7"), -1);
    EXPECT_EQ(parseMuxDeviceRootBus(""), -1);
}
