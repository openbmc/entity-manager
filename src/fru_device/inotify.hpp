#pragma once

#include <sys/inotify.h>

#include <phosphor-logging/lg2.hpp>

#include <cerrno>
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
