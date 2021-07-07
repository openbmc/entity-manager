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
/// \file devices.hpp

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
    ExportTemplate(const char* params, const char* dev, const char* constructor,
                   const char* destructor, bool createsHWMon) :
        parameters(params),
        devicePath(dev), add(constructor), remove(destructor),
        createsHWMon(createsHWMon){};
    const char* parameters;
    const char* devicePath;
    const char* add;
    const char* remove;
    bool createsHWMon;
};

const boost::container::flat_map<const char*, ExportTemplate, CmpStr>
    exportTemplates{
        {{"EEPROM_24C01",
          ExportTemplate("24c01 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"EEPROM_24C02",
          ExportTemplate("24c02 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"EEPROM_24C64",
          ExportTemplate("24c64 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"ADM1266",
          ExportTemplate("adm1266 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"ADM1272",
          ExportTemplate("adm1272 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"DPS310",
          ExportTemplate("dps310 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"SI7020",
          ExportTemplate("si7020 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"EEPROM",
          ExportTemplate("eeprom $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"EMC1412",
          ExportTemplate("emc1412 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"EMC1413",
          ExportTemplate("emc1413 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"EMC1414",
          ExportTemplate("emc1414 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"Gpio", ExportTemplate("$Index", "/sys/class/gpio", "export",
                                 "unexport", false)},
         {"INA230",
          ExportTemplate("ina230 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"ISL68137",
          ExportTemplate("isl68137 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"ISL68223",
          ExportTemplate("isl68223 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"ISL69243",
          ExportTemplate("isl69243 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"MAX16601",
          ExportTemplate("max16601 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"MAX20710",
          ExportTemplate("max20710 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"MAX20730",
          ExportTemplate("max20730 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"MAX20734",
          ExportTemplate("max20734 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"MAX20796",
          ExportTemplate("max20796 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"MAX31725",
          ExportTemplate("max31725 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"MAX31730",
          ExportTemplate("max31730 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"MAX34440",
          ExportTemplate("max34440 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"MAX34451",
          ExportTemplate("max34451 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"MAX6654",
          ExportTemplate("max6654 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"PCA9543Mux",
          ExportTemplate("pca9543 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"PCA9544Mux",
          ExportTemplate("pca9544 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"PCA9545Mux",
          ExportTemplate("pca9545 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"PCA9546Mux",
          ExportTemplate("pca9546 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"PCA9547Mux",
          ExportTemplate("pca9547 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", false)},
         {"SBTSI",
          ExportTemplate("sbtsi $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"pmbus",
          ExportTemplate("pmbus $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"LM95234",
          ExportTemplate("lm95234 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"TMP112",
          ExportTemplate("tmp112 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"TMP175",
          ExportTemplate("tmp175 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"TMP421",
          ExportTemplate("tmp421 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"TMP441",
          ExportTemplate("tmp441 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"LM75A",
          ExportTemplate("lm75a $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"TMP75",
          ExportTemplate("tmp75 $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)},
         {"W83773G",
          ExportTemplate("w83773g $Address", "/sys/bus/i2c/devices/i2c-$Bus",
                         "new_device", "delete_device", true)}}};
} // namespace devices
