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

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <regex>
#include <string>

constexpr const char* outputDir = "/tmp/overlays";
constexpr const char* templateChar = "$";
constexpr const char* i2CDevsDir = "/sys/bus/i2c/devices";
constexpr const char* muxSymlinkDir = "/dev/i2c-mux";

constexpr const bool debug = false;

std::regex illegalNameRegex("[^A-Za-z0-9_]");

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

void linkMux(const std::string& muxName, size_t busIndex, size_t address,
             const nlohmann::json::array_t& channelNames)
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
            std::cerr << channelPath << " for mux channel " << *channelName
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

static int deleteDevice(const std::string& busPath,
                        const std::shared_ptr<uint64_t>& address,
                        const std::string& destructor)
{
    if (!address)
    {
        return -1;
    }
    std::filesystem::path deviceDestructor(busPath);
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

static bool deviceIsCreated(const std::string& busPath,
                            const std::shared_ptr<uint64_t>& bus,
                            const std::shared_ptr<uint64_t>& address,
                            const devices::createsHWMon hasHWMonDir)
{
    if (!bus || !address)
    {
        return false;
    }

    std::filesystem::path dirPath = busPath;
    dirPath /= deviceDirName(*bus, *address);
    if (hasHWMonDir == devices::createsHWMon::hasHWMonDir)
    {
        dirPath /= "hwmon";
    }

    std::error_code ec;
    // Ignore errors; anything but a clean 'true' is just fine as 'false'
    return std::filesystem::exists(dirPath, ec);
}

static int buildDevice(const std::string& busPath,
                       const std::string& parameters,
                       const std::shared_ptr<uint64_t>& bus,
                       const std::shared_ptr<uint64_t>& address,
                       const std::string& constructor,
                       const std::string& destructor,
                       const devices::createsHWMon hasHWMonDir,
                       const size_t retries = 5)
{
    if (!retries)
    {
        return -1;
    }

    // If it's already instantiated, there's nothing we need to do.
    if (deviceIsCreated(busPath, bus, address, hasHWMonDir))
    {
        return 0;
    }

    // Try to create the device
    createDevice(busPath, parameters, constructor);

    // If it didn't work, delete it and try again in 500ms
    if (!deviceIsCreated(busPath, bus, address, hasHWMonDir))
    {
        deleteDevice(busPath, address, destructor);

        std::shared_ptr<boost::asio::steady_timer> createTimer =
            std::make_shared<boost::asio::steady_timer>(io);
        createTimer->expires_after(std::chrono::milliseconds(500));
        createTimer->async_wait([createTimer, busPath, parameters, bus, address,
                                 constructor, destructor, hasHWMonDir,
                                 retries](const boost::system::error_code& ec) {
            if (ec)
            {
                std::cerr << "Timer error: " << ec << "\n";
                return -2;
            }
            return buildDevice(busPath, parameters, bus, address, constructor,
                               destructor, hasHWMonDir, retries - 1);
        });
    }

    return 0;
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
                keyPair.value().get<std::string>(), illegalNameRegex, "_");
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
        boost::replace_all(parameters, templateChar + keyPair.key(),
                           subsituteString);
        boost::replace_all(busPath, templateChar + keyPair.key(),
                           subsituteString);
    }

    int err = buildDevice(busPath, parameters, bus, address, constructor,
                          destructor, hasHWMonDir);

    if (!err && boost::ends_with(type, "Mux") && bus && address && channels)
    {
        linkMux(name, static_cast<size_t>(*bus), static_cast<size_t>(*address),
                *channels);
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
            if constexpr (debug)
            {
                std::cerr << "Device type " << type
                          << " not found in export map whitelist\n";
            }
        }
    }

    return true;
}
