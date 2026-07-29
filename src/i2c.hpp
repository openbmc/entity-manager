#pragma once

#include <compare>
#include <cstdint>
#include <filesystem>
#include <string>

class I2cBusNum
{
  public:
    constexpr explicit I2cBusNum(size_t value) noexcept : value(value) {}

    [[nodiscard]] constexpr size_t get() const noexcept
    {
        return value;
    }

    std::string string() const
    {
        return std::to_string(value);
    }

    std::filesystem::path sysfsPath() const
    {
        return "/sys/bus/i2c/devices/i2c-" + string();
    }

    std::filesystem::path devfsPath() const
    {
        return "/dev/i2c-" + string();
    }

    friend constexpr auto operator<=>(const I2cBusNum&,
                                      const I2cBusNum&) noexcept = default;

  private:
    size_t value;
};
