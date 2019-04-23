/*
// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#pragma once
#include <boost/container/flat_map.hpp>

namespace devices
{

struct CmpStr
{
    bool operator()(const char* a, const char* b) const
    {
        return std::strcmp(a, b) < 0;
    }
};

struct ExportTemplate
{
    ExportTemplate(const char* parameters, const char* device) :
        parameters(parameters), device(device){};
    const char* parameters;
    const char* device;
};

const boost::container::flat_map<const char*, ExportTemplate, CmpStr>
    exportTemplates{
        {{"EEPROM", ExportTemplate("eeprom $Address",
                                   "/sys/bus/i2c/devices/i2c-$Bus/new_device")},
         {"Gpio", ExportTemplate("$Index", "/sys/class/gpio/export")},
         {"PCA9543Mux",
          ExportTemplate("pca9543 $Address",
                         "/sys/bus/i2c/devices/i2c-$Bus/new_device")},
         {"PCA9544Mux",
          ExportTemplate("pca9544 $Address",
                         "/sys/bus/i2c/devices/i2c-$Bus/new_device")},
         {"PCA9545Mux",
          ExportTemplate("pca9545 $Address",
                         "/sys/bus/i2c/devices/i2c-$Bus/new_device")},
         {"PCA9546Mux",
          ExportTemplate("pca9546 $Address",
                         "/sys/bus/i2c/devices/i2c-$Bus/new_device")},
         {"pmbus", ExportTemplate("pmbus $Address",
                                  "/sys/bus/i2c/devices/i2c-$Bus/new_device")},
         {"TMP75", ExportTemplate("tmp75 $Address",
                                  "/sys/bus/i2c/devices/i2c-$Bus/new_device")},
         {"TMP421", ExportTemplate("tmp421 $Address",
                                   "/sys/bus/i2c/devices/i2c-$Bus/new_device")},
         {"EMC1413",
          ExportTemplate("emc1413 $Address",
                         "/sys/bus/i2c/devices/i2c-$Bus/new_device")},
         {"TMP112", ExportTemplate("tmp112 $Address",
                                   "/sys/bus/i2c/devices/i2c-$Bus/new_device")},
         {"XeonCPU",
          ExportTemplate("peci-client $Address",
                         "/sys/bus/peci/devices/peci-$Bus/new_device")

         }}};
} // namespace devices
