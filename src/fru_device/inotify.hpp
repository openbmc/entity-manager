#pragma once

#include <sys/inotify.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <optional>

inline std::optional<int> createInotify()
{
    int fd = inotify_init();
    if (fd < 0)
    {
        lg2::error("Failed to initialize inotify: {ERROR}", "ERROR",
                   std::strerror(errno));
        return std::nullopt;
    }

    return fd;
}

inline std::optional<int> createInotifyWatch(const char* path, uint32_t mask)
{
    auto fd = createInotify();
    if (!fd)
    {
        return std::nullopt;
    }

    if (inotify_add_watch(*fd, path, mask) < 0)
    {
        lg2::error("Failed to watch {PATH}: {ERROR}", "PATH", path, "ERROR",
                   std::strerror(errno));
        if (close(*fd) < 0)
        {
            lg2::error("Failed to close inotify descriptor: {ERROR}", "ERROR",
                       std::strerror(errno));
        }
        return std::nullopt;
    }

    return fd;
}
