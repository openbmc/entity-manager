// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026

#include "eeprom_utils.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <phosphor-logging/lg2.hpp>

#include <cerrno>
#include <cstdint>
#include <cstring>

EepromWriteResult writeEepromFile(int fd, const std::filesystem::path& path,
                                  off_t offset, std::span<const uint8_t> data)
{
    const off_t position = lseek(fd, offset, SEEK_SET);
    if (position < 0)
    {
        const int error = errno;
        lg2::error("Unable to seek to offset {OFFSET} in EEPROM device {PATH}: "
                   "{ERROR}",
                   "OFFSET", offset, "PATH", path.string(), "ERROR",
                   std::strerror(error));
        return EepromWriteResult::seekError;
    }

    struct stat info{};
    if (fstat(fd, &info) < 0)
    {
        const int error = errno;
        lg2::error("Unable to get EEPROM size for {PATH}: {ERROR}", "PATH",
                   path.string(), "ERROR", std::strerror(error));
        return EepromWriteResult::writeError;
    }

    // A sysfs binary attribute can report zero when its size is unspecified.
    if (info.st_size > 0 &&
        (position > info.st_size ||
         static_cast<std::uintmax_t>(data.size()) >
             static_cast<std::uintmax_t>(info.st_size - position)))
    {
        lg2::error(
            "FRU size {SIZE} at offset {OFFSET} exceeds EEPROM size {CAPACITY} "
            "for {PATH}",
            "SIZE", data.size(), "OFFSET", position, "CAPACITY", info.st_size,
            "PATH", path.string());
        return EepromWriteResult::writeError;
    }

    const ssize_t written = write(fd, data.data(), data.size());
    if (written < 0)
    {
        const int error = errno;
        lg2::error("EEPROM write failed for {PATH}: {ERROR}", "PATH",
                   path.string(), "ERROR", std::strerror(error));
        return EepromWriteResult::writeError;
    }
    if (static_cast<size_t>(written) != data.size())
    {
        lg2::error(
            "EEPROM write incomplete for {PATH}: expected {EXPECTED}, wrote "
            "{ACTUAL}",
            "PATH", path.string(), "EXPECTED", data.size(), "ACTUAL", written);
        return EepromWriteResult::writeError;
    }
    return EepromWriteResult::success;
}
