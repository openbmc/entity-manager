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
/// \file Overlay.cpp

#include "Overlay.hpp"

#include "Utils.hpp"
#include "devices.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/process/child.hpp>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <regex>
#include <string>

constexpr const char* OUTPUT_DIR = "/tmp/overlays";
constexpr const char* TEMPLATE_CHAR = "$";
constexpr const char* HEX_FORMAT_STR = "0x";
constexpr const char* I2C_DEVS_DIR = "/sys/bus/i2c/devices";
constexpr const char* MUX_SYMLINK_DIR = "/dev/i2c-mux";

constexpr const bool DEBUG = false;

std::regex ILLEGAL_NAME_REGEX("[^A-Za-z0-9_]");

// helper function to make json types into string
std::string jsonToString(const nlohmann::json& in)
{
    if (in.type() == nlohmann::json::value_t::string)
    {
        return in.get<std::string>();
    }
    else if (in.type() == nlohmann::json::value_t::array)
    {
        // remove brackets and comma from array
        std::string array = in.dump();
        array = array.substr(1, array.size() - 2);
        boost::replace_all(array, ",", " ");
        return array;
    }
    return in.dump();
}

void linkMux(const std::string& muxName, size_t busIndex, size_t address,
             const nlohmann::json::array_t& channelNames)
{
    std::error_code ec;
    std::filesystem::path muxSymlinkDir(MUX_SYMLINK_DIR);
    std::filesystem::create_directory(muxSymlinkDir, ec);
    // ignore error codes here if the directory already exists
    ec.clear();
    std::filesystem::path linkDir = muxSymlinkDir / muxName;
    std::filesystem::create_directory(linkDir, ec);

    std::ostringstream hexAddress;
    hexAddress << std::hex << std::setfill('0') << std::setw(4) << address;

    std::filesystem::path devDir(I2C_DEVS_DIR);
    devDir /= std::to_string(busIndex) + "-" + hexAddress.str();

    for (std::size_t channelIndex = 0; channelIndex < channelNames.size();
         channelIndex++)
    {
        const std::string* channelName =
            channelNames[channelIndex].get_ptr<const std::string*>();
        if (channelName == nullptr)
        {
            continue;
        }
        if (channelName->empty())
        {
            continue;
        }

        std::filesystem::path channelPath =
            devDir / ("channel-" + std::to_string(channelIndex));
        if (!is_symlink(channelPath))
        {
            std::cerr << channelPath << "for mux channel " << *channelName
                      << " doesn't exist!\n";
            continue;
        }
        std::filesystem::path bus = std::filesystem::read_symlink(channelPath);

        std::filesystem::path fp("/dev" / bus.filename());
        std::filesystem::path link(linkDir / *channelName);

        std::filesystem::create_symlink(fp, link, ec);
        if (ec)
        {
            std::cerr << "Failure creating symlink for " << fp << " to " << link
                      << "\n";
        }
    }
}

static int deleteDevice(const std::string& devicePath,
                        std::shared_ptr<uint64_t> address,
                        const std::string& destructor)
{
    if (!address)
    {
        return -1;
    }
    std::filesystem::path deviceDestructor(devicePath);
    deviceDestructor /= destructor;
    std::ofstream deviceFile(deviceDestructor);
    if (!deviceFile.good())
    {
        std::cerr << "Error writing " << deviceDestructor << "\n";
        return -1;
    }
    deviceFile << std::to_string(*address);
    deviceFile.close();
    return 0;
}

static int createDevice(const std::string& devicePath,
                        const std::string& parameters,
                        const std::string& constructor)
{
    std::filesystem::path deviceConstructor(devicePath);
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

static bool deviceIsCreated(const std::string& devicePath,
                            std::shared_ptr<uint64_t> bus,
                            std::shared_ptr<uint64_t> address,
                            const bool retrying)
{
    if (!bus || !address)
    {
        return false;
    }

    std::ostringstream hex;
    hex << std::hex << std::setw(4) << std::setfill('0') << *address;
    std::string addressHex = hex.str();
    std::string busStr = std::to_string(*bus);

    for (auto path = std::filesystem::recursive_directory_iterator(devicePath);
         path != std::filesystem::recursive_directory_iterator(); path++)
    {
        if (!std::filesystem::is_directory(*path))
        {
            continue;
        }

        const std::string directoryName = path->path().filename();
        if (directoryName == busStr + "-" + addressHex)
        {
            // The first time the BMC boots the kernel has creates a
            // filesystem enumerating the I2C devices. The I2C device has not
            // been initialized for use. This requires a call to a device
            // node, such as "new_device". The first pass through this
            // function is only confirming the filesystem contains the device
            // entry of interest (i.e. i2c4-0050).
            //
            // An upper level function performs the device creation
            // action. This action may fail. The device driver (dd) used to
            // create the I2C filesystem substructure eats any error codes,
            // and always returns 0. This is by design. It is also possible
            // for the new_device action to fail because the device is not
            // actually in the system, i.e. optional equipment.
            //
            // The 'retrying' pass of this function is used to confirm the
            // 'dd' device driver succeeded. Success is measured by finding
            // the 'hwmon' subdirectory in the filesystem. The first attempt
            // is delayed by an arbitrary amount, in order to permit the
            // kernel time to create the filesystem entries. The upper level
            // function determines the number of times to retry calling this
            // function.
            if (retrying)
            {
                std::error_code ec;
                std::filesystem::path hwmonDir(devicePath);
                hwmonDir /= directoryName;
                hwmonDir /= "hwmon";
                return std::filesystem::is_directory(hwmonDir, ec);
            }
            return true;
        }
        else
        {
            path.disable_recursion_pending();
        }
    }
    return false;
}

static int buildDevice(const std::string& devicePath,
                       const std::string& parameters,
                       std::shared_ptr<uint64_t> bus,
                       std::shared_ptr<uint64_t> address,
                       const std::string& constructor,
                       const std::string& destructor, const bool createsHWMon,
                       const size_t retries = 5)
{
    bool tryAgain = false;
    if (!retries)
    {
        return -1;
    }

    if (!deviceIsCreated(devicePath, bus, address, false))
    {
        createDevice(devicePath, parameters, constructor);
        tryAgain = true;
    }
    else if (createsHWMon && !deviceIsCreated(devicePath, bus, address, true))
    {
        // device is present, hwmon subdir missing
        deleteDevice(devicePath, address, destructor);
        tryAgain = true;
    }

    if (tryAgain)
    {
        std::shared_ptr<boost::asio::steady_timer> createTimer =
            std::make_shared<boost::asio::steady_timer>(io);
        createTimer->expires_after(std::chrono::milliseconds(500));
        createTimer->async_wait([createTimer, devicePath, parameters, bus,
                                 address, constructor, destructor, createsHWMon,
                                 retries](const boost::system::error_code& ec) {
            if (ec)
            {
                std::cerr << "Timer error: " << ec << "\n";
                return -2;
            }
            return buildDevice(devicePath, parameters, bus, address,
                               constructor, destructor, createsHWMon,
                               retries - 1);
        });
    }
    return 0;
}

void exportDevice(const std::string& type,
                  const devices::ExportTemplate& exportTemplate,
                  const nlohmann::json& configuration)
{

    std::string parameters = exportTemplate.parameters;
    std::string devicePath = exportTemplate.devicePath;
    std::string constructor = exportTemplate.add;
    std::string destructor = exportTemplate.remove;
    bool createsHWMon = exportTemplate.createsHWMon;
    std::string name = "unknown";
    std::shared_ptr<uint64_t> bus = nullptr;
    std::shared_ptr<uint64_t> address = nullptr;
    const nlohmann::json::array_t* channels = nullptr;

    for (auto keyPair = configuration.begin(); keyPair != configuration.end();
         keyPair++)
    {
        std::string subsituteString;

        if (keyPair.key() == "Name" &&
            keyPair.value().type() == nlohmann::json::value_t::string)
        {
            subsituteString = std::regex_replace(
                keyPair.value().get<std::string>(), ILLEGAL_NAME_REGEX, "_");
            name = subsituteString;
        }
        else
        {
            subsituteString = jsonToString(keyPair.value());
        }

        if (keyPair.key() == "Bus")
        {
            bus = std::make_shared<uint64_t>(
                *keyPair.value().get_ptr<const uint64_t*>());
        }
        else if (keyPair.key() == "Address")
        {
            address = std::make_shared<uint64_t>(
                *keyPair.value().get_ptr<const uint64_t*>());
        }
        else if (keyPair.key() == "ChannelNames")
        {
            channels =
                keyPair.value().get_ptr<const nlohmann::json::array_t*>();
        }
        boost::replace_all(parameters, TEMPLATE_CHAR + keyPair.key(),
                           subsituteString);
        boost::replace_all(devicePath, TEMPLATE_CHAR + keyPair.key(),
                           subsituteString);
    }

    int err = buildDevice(devicePath, parameters, bus, address, constructor,
                          destructor, createsHWMon);

    if (!err && boost::ends_with(type, "Mux") && bus && address && channels)
    {
        linkMux(name, static_cast<size_t>(*bus), static_cast<size_t>(*address),
                *channels);
    }
}

bool loadOverlays(const nlohmann::json& systemConfiguration)
{
    std::filesystem::create_directory(OUTPUT_DIR);
    for (auto entity = systemConfiguration.begin();
         entity != systemConfiguration.end(); entity++)
    {
        auto findExposes = entity.value().find("Exposes");
        if (findExposes == entity.value().end() ||
            findExposes->type() != nlohmann::json::value_t::array)
        {
            continue;
        }

        for (auto& configuration : *findExposes)
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
            if constexpr (DEBUG)
            {
                std::cerr << "Device type " << type
                          << " not found in export map whitelist\n";
            }
        }
    }

    return true;
}
