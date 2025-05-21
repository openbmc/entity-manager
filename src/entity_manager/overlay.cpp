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
/// \file overlay.cpp

#include "overlay.hpp"

#include "devices.hpp"
#include "utils.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/process/child.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <string>

constexpr const char* outputDir = "/tmp/overlays";
constexpr const char* templateChar = "$";
constexpr const char* i2CDevsDir = "/sys/bus/i2c/devices";
constexpr const char* muxSymlinkDir = "/dev/i2c-mux";
const std::regex illegalNameRegex("[^A-Za-z0-9_]");

// helper function to make json types into string
std::string jsonToString(const nlohmann::json& in)
{
    if (in.type() == nlohmann::json::value_t::string)
    {
        return in.get<std::string>();
    }
    if (in.type() == nlohmann::json::value_t::array)
    {
        // remove brackets and comma from array
        std::string array = in.dump();
        array = array.substr(1, array.size() - 2);
        boost::replace_all(array, ",", " ");
        return array;
    }
    return in.dump();
}

static std::string deviceDirName(uint64_t bus, uint64_t address)
{
    std::ostringstream name;
    name << bus << "-" << std::hex << std::setw(4) << std::setfill('0')
         << address;
    return name.str();
}

void linkMux(const std::string& muxName, uint64_t busIndex, uint64_t address,
             const std::vector<std::string>& channelNames)
{
    std::error_code ec;
    std::filesystem::path muxSymlinkDirPath(muxSymlinkDir);
    std::filesystem::create_directory(muxSymlinkDirPath, ec);
    // ignore error codes here if the directory already exists
    ec.clear();
    std::filesystem::path linkDir = muxSymlinkDirPath / muxName;
    std::filesystem::create_directory(linkDir, ec);

    std::filesystem::path devDir(i2CDevsDir);
    devDir /= deviceDirName(busIndex, address);

    for (std::size_t channelIndex = 0; channelIndex < channelNames.size();
         channelIndex++)
    {
        const std::string& channelName = channelNames[channelIndex];
        if (channelName.empty())
        {
            continue;
        }

        std::filesystem::path channelPath =
            devDir / ("channel-" + std::to_string(channelIndex));
        if (!is_symlink(channelPath))
        {
            std::cerr << channelPath << " for mux channel " << channelName
                      << " doesn't exist!\n";
            continue;
        }
        std::filesystem::path bus = std::filesystem::read_symlink(channelPath);

        std::filesystem::path fp("/dev" / bus.filename());
        std::filesystem::path link(linkDir / channelName);

        std::filesystem::create_symlink(fp, link, ec);
        if (ec)
        {
            std::cerr << "Failure creating symlink for " << fp << " to " << link
                      << "\n";
        }
    }
}

static int deleteDevice(const std::string& busPath, uint64_t address,
                        const std::string& destructor)
{
    std::filesystem::path deviceDestructor(busPath);
    deviceDestructor /= destructor;
    std::ofstream deviceFile(deviceDestructor);
    if (!deviceFile.good())
    {
        std::cerr << "Error writing " << deviceDestructor << "\n";
        return -1;
    }
    deviceFile << std::to_string(address);
    deviceFile.close();
    return 0;
}

static int createDevice(const std::string& busPath,
                        const std::string& parameters,
                        const std::string& constructor)
{
    std::filesystem::path deviceConstructor(busPath);
    deviceConstructor /= constructor;
    std::ofstream deviceFile(deviceConstructor);
    if (!deviceFile.good())
    {
        std::cerr << "Error writing " << deviceConstructor << "\n";
        return -1;
    }
    deviceFile << parameters;
    deviceFile.close();

    return 0;
}

static bool deviceIsCreated(const std::string& busPath, uint64_t bus,
                            uint64_t address,
                            const devices::createsHWMon hasHWMonDir)
{
    std::filesystem::path dirPath = busPath;
    dirPath /= deviceDirName(bus, address);
    if (hasHWMonDir == devices::createsHWMon::hasHWMonDir)
    {
        dirPath /= "hwmon";
    }

    std::error_code ec;
    // Ignore errors; anything but a clean 'true' is just fine as 'false'
    return std::filesystem::exists(dirPath, ec);
}

static int dynamicDeviceCreation(
    const std::string& name, const std::string& busPath,
    const std::string& parameters, uint64_t bus, uint64_t address,
    const std::string& constructor, const std::string& destructor,
    const devices::createsHWMon hasHWMonDir,
    std::vector<std::string> channelNames, const size_t retries = 5)
{
    if (retries == 0U)
    {
        return -1;
    }

    // If it's already instantiated, we don't need to create it again.
    if (!deviceIsCreated(busPath, bus, address, hasHWMonDir))
    {
        // Try to create the device
        createDevice(busPath, parameters, constructor);

        // If it didn't work, delete it and try again in 500ms
        if (!deviceIsCreated(busPath, bus, address, hasHWMonDir))
        {
            deleteDevice(busPath, address, destructor);

            std::shared_ptr<boost::asio::steady_timer> createTimer =
                std::make_shared<boost::asio::steady_timer>(io);
            createTimer->expires_after(std::chrono::milliseconds(500));
            createTimer->async_wait(
                [createTimer, name, busPath, parameters, bus, address,
                 constructor, destructor, hasHWMonDir,
                 channelNames(std::move(channelNames)),
                 retries](const boost::system::error_code& ec) mutable {
                    if (ec)
                    {
                        std::cerr << "Timer error: " << ec << "\n";
                        return -2;
                    }
                    return dynamicDeviceCreation(
                        name, busPath, parameters, bus, address, constructor,
                        destructor, hasHWMonDir, std::move(channelNames),
                        retries - 1);
                });
            return -1;
        }
    }

    // Link the mux channels if needed once the device is created.
    if (!channelNames.empty())
    {
        linkMux(name, bus, address, channelNames);
    }

    return 0;
}

/**
 * @brief Create a device tree overlay file to overlay a GPIO device
 *
 * @param[in] name - name of the device node
 * @param[in] busPath - name of i2c bus node
 * @param[in] parameters - name of device driver
 * @param[in] bus - bus number
 * @param[in] address - address of the device
 * @param[in] gpios - gpios for the device
 * @param[in] interruptPhandle - phandle of the interrupt parent
 * @param[in] interruptPin - pin number of the interrupt
 * @param[in] interruptType - type of the interrupt
 * @param[out] dtsPath - path to the dts file
 *
 * @return 0 on success, -1 on failure
 */
static int createDTSOFile(
    const std::string& name, const std::string& busPath,
    const std::string& parameters, const uint64_t bus, const uint64_t address,
    const std::vector<std::string> gpios, const std::string& interruptPhandle,
    const uint8_t interruptPin, const std::string& interruptType,
    std::string& dtsoPath)
{
    std::string working_dir = "/tmp/entity_manager/";
    std::filesystem::create_directories(working_dir);
    std::string overlay_file_path = working_dir + name + "_overlay.dts";
    std::ofstream overlay_file(overlay_file_path);
    if (!overlay_file)
    {
        std::cerr << "Failed to create overlay file: " << overlay_file_path
                  << std::endl;
        return -1;
    }

    overlay_file << std::format("// SPDX-License-Identifier: GPL-2.0+\n");
    overlay_file << std::format("\n");
    overlay_file << std::format("{} {{\n", busPath); // open i2c
    overlay_file << std::format("\t{}: gpio@{} {{\n", name,
                                address);            // open gpio

    overlay_file << std::format("\t\tcompatible = \"{}\";\n", parameters);
    overlay_file << std::format("\t\treg = <0x{}>;\n", address);
    overlay_file << std::format("\t\tgpio-controller;\n");
    overlay_file << std::format("\t\t#gpio-cells = <2>;\n");
    overlay_file << std::format("\t\tinterrupt-controller;\n", address);
    overlay_file << std::format("\t\t#interrupt-cells = <2>;\n", address);
    overlay_file << std::format("\t\tinterrupt-parent = <{}>;\n",
                                interruptPhandle);
    overlay_file << std::format("\t\tinterrupts = <{} {}>;\n", interruptPin,
                                interruptType);
    overlay_file << std::format("\t\tgpio-line-names =\n");

    for (const auto& gpio : gpios)
    {
        overlay_file << std::format("\t\t\t\"{}\",\n", gpio);
    }

    overlay_file << std::format("\t}};\n", bus); // close gpio
    overlay_file << std::format("}};\n", bus);   // close i2c

    overlay_file.close();
    dtsoPath = overlay_file_path;
    std::cout << "Overlay file created at: " << overlay_file_path << std::endl;
    return 0;
}

static int compileDeviceTree(const std::string& dtsoPath, std::string& dtboPath)
{
    // import lib device tree 
    // dtc -@ -I dts -O dtb -o my-overlay.dtbo my-overlay.dtso

    return 0;
}

static int applyDeviceTreeBlob(const std::string& dtboPath)
{
    // import lib device tree 
    // require beaglebone patch
    return 0;
}

/**
 * @brief Adds a GPIO expander using device tree overlays
 *
 * @param[in] name - name of the device node
 * @param[in] busPath - name of i2c bus node
 * @param[in] parameters - name of device driver
 * @param[in] bus - bus number
 * @param[in] address - address of the device
 * @param[in] gpios - gpios for the device
 * @param[in] interruptPhandle - phandle of the interrupt parent
 * @param[in] interruptPin - pin number of the interrupt
 * @param[in] interruptType - type of the interrupt
 *
 * @return 0 on success, -1 on failure
 */
static int deviceTreeOverlay(
    const std::string& name, const std::string& busPath,
    const std::string& parameters, const uint64_t bus, const uint64_t address,
    const std::vector<std::string> gpios, const std::string& interruptPhandle,
    const uint8_t interruptPin, const std::string& interruptType)
{
    std::string dtsoPath;
    createDTSOFile(name, busPath, parameters, bus, address, gpios,
                   interruptPhandle, interruptPin, interruptType, dtsoPath);

    std::string dtboPath;
    compileDeviceTree(dtsoPath, dtboPath);

    applyDeviceTreeBlob(dtboPath);
}

void exportDevice(const std::string& type,
                  const devices::ExportTemplate& exportTemplate,
                  const nlohmann::json& configuration)
{
    std::string parameters = exportTemplate.parameters;
    std::string busPath = exportTemplate.busPath;
    std::string constructor = exportTemplate.add;
    std::string destructor = exportTemplate.remove;
    devices::createsHWMon hasHWMonDir = exportTemplate.hasHWMonDir;
    std::string name = "unknown";
    std::optional<uint64_t> bus;
    std::optional<uint64_t> address;
    std::optional<std::string> interruptPhandle;
    std::optional<uint8_t> interruptPin;
    std::optional<std::string> interruptType;
    std::vector<std::string> channels;
    std::vector<std::string> gpios;

    for (auto keyPair = configuration.begin(); keyPair != configuration.end();
         keyPair++)
    {
        std::string subsituteString;

        if (keyPair.key() == "Name" &&
            keyPair.value().type() == nlohmann::json::value_t::string)
        {
            subsituteString = std::regex_replace(
                keyPair.value().get<std::string>(), illegalNameRegex, "_");
            name = subsituteString;
        }
        else
        {
            subsituteString = jsonToString(keyPair.value());
        }

        if (keyPair.key() == "Bus")
        {
            bus = keyPair.value().get<uint64_t>();
        }
        else if (keyPair.key() == "Address")
        {
            address = keyPair.value().get<uint64_t>();
        }
        else if (keyPair.key() == "ChannelNames" && type.ends_with("Mux"))
        {
            channels = keyPair.value().get<std::vector<std::string>>();
        }
        else if (keyPair.key() == "GpioNames" && type.ends_with("Iox"))
        {
            gpios = keyPair.value().get<std::vector<std::string>>();
        }
        else if (keyPair.key() == "InterruptPin" && type.ends_with("Iox"))
        {
            interruptPhandle = keyPair.value().find("Phandle");
            interruptPin = keyPair.value().find("Pin");
            interruptType = keyPair.value().find("Type");
        }
        boost::replace_all(parameters, templateChar + keyPair.key(),
                           subsituteString);
        boost::replace_all(busPath, templateChar + keyPair.key(),
                           subsituteString);
    }

    if (!bus || !address)
    {
        createDevice(busPath, parameters, constructor);
        return;
    }
    else if (interruptPhandle && interruptPin && interruptType &&
             !gpios.empty())
    {
        deviceTreeOverlay(name, busPath, parameters, *bus, *address,
                          std::move(gpios), interruptPhandle.value(),
                          interruptPin.value(), interruptType.value());
        return;
    }
    else
    {
        dynamicDeviceCreation(name, busPath, parameters, *bus, *address,
                              constructor, destructor, hasHWMonDir,
                              std::move(channels), std::move(gpios));
    }
}

bool loadOverlays(const nlohmann::json& systemConfiguration)
{
    std::filesystem::create_directory(outputDir);
    for (auto entity = systemConfiguration.begin();
         entity != systemConfiguration.end(); entity++)
    {
        auto findExposes = entity.value().find("Exposes");
        if (findExposes == entity.value().end() ||
            findExposes->type() != nlohmann::json::value_t::array)
        {
            continue;
        }

        for (const auto& configuration : *findExposes)
        {
            auto findStatus = configuration.find("Status");
            // status missing is assumed to be 'okay'
            if (findStatus != configuration.end() && *findStatus == "disabled")
            {
                continue;
            }
            auto findType = configuration.find("Type");
            if (findType == configuration.end() ||
                findType->type() != nlohmann::json::value_t::string)
            {
                continue;
            }
            std::string type = findType.value().get<std::string>();
            auto device = devices::exportTemplates.find(type.c_str());
            if (device != devices::exportTemplates.end())
            {
                exportDevice(type, device->second, configuration);
                continue;
            }

            // Because many devices are intentionally not exportable,
            // this error message is not printed in all situations.
            // If wondering why your device not appearing, add your type to
            // the exportTemplates array in the devices.hpp file.
            lg2::debug("Device type {TYPE} not found in export map allowlist",
                       "TYPE", type);
        }
    }

    return true;
}
