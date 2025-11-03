// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once
#include <array>

namespace devices
{

// I2C device drivers may create a /hwmon subdirectory. For example the tmp75
// driver creates a /sys/bus/i2c/devices/<busnum>-<i2caddr>/hwmon
// directory. The sensor code relies on the presence of the /hwmon
// subdirectory to collect sensor readings. Initialization of this subdir is
// not reliable. I2C devices flagged with hasHWMonDir are tested for correct
// initialization, and when a failure is detected the device is deleted, and
// then recreated. The default is to retry 5 times before moving to the next
// device.

// Devices such as I2C EEPROMs do not generate this file structure. These
// kinds of devices are flagged using the noHWMonDir enumeration. The
// expectation is they are created correctly on the first attempt.

// This enumeration class exists to reduce copy/paste errors. It is easy to
// overlook the trailing parameter in the ExportTemplate structure when it is
// a simple boolean.
enum class createsHWMon : bool
{
    noHWMonDir,
    hasHWMonDir
};

struct ExportTemplate
{
    std::string_view type;
    std::string_view parameters;
    std::string_view busPath;
    std::string_view add;
    std::string_view remove;
    createsHWMon hasHWMonDir;
};

constexpr auto exportTemplates = std::to_array<ExportTemplate>(
    {{"EEPROM_24C01", "24c01 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"EEPROM_24C02", "24c02 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"EEPROM_24C04", "24c04 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"EEPROM_24C08", "24c08 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"EEPROM_24C16", "24c16 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"EEPROM_24C32", "24c32 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"EEPROM_24C64", "24c64 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"EEPROM_24C128", "24c128 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"EEPROM_24C256", "24c256 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"ADS1015", "ads1015 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"ADS7828", "ads7828 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"EEPROM", "eeprom $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"Gpio", "$Index", "/sys/class/gpio", "export", "unexport",
      createsHWMon::noHWMonDir},
     {"IPSPS1", "ipsps1 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::hasHWMonDir},
     {"PCF85053", "pcf85053 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"MAX34440", "max34440 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::hasHWMonDir},
     {"PCA9537", "pca9537 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"PCA9542Mux", "pca9542 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"PCA9543Mux", "pca9543 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"PCA9544Mux", "pca9544 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"PCA9545Mux", "pca9545 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"PCA9546Mux", "pca9546 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"PCA9547Mux", "pca9547 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"PCA9548Mux", "pca9548 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"PCA9846Mux", "pca9846 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"PCA9847Mux", "pca9847 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"PCA9848Mux", "pca9848 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"PCA9849Mux", "pca9849 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"SIC450", "sic450 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::hasHWMonDir},
     {"Q50SN12072", "q50sn12072 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::hasHWMonDir},
     {"MAX31790", "max31790 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::hasHWMonDir},
     {"PIC32", "pic32 $Address", "/sys/bus/i2c/devices/i2c-$Bus", "new_device",
      "delete_device", createsHWMon::hasHWMonDir},
     {"INA226", "ina226 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::hasHWMonDir},
     {"RAA229620", "raa229620 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::hasHWMonDir},
     {"RAA229621", "raa229621 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::hasHWMonDir},
     {"PIC32", "pic32 $Address", "/sys/bus/i2c/devices/i2c-$Bus", "new_device",
      "delete_device", createsHWMon::hasHWMonDir},
     {"INA233", "ina233 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::hasHWMonDir},
     {"RTQ6056", "rtq6056 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::noHWMonDir},
     {"SY24655", "sy24655 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
      "new_device", "delete_device", createsHWMon::hasHWMonDir}});
} // namespace devices
