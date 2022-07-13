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

#include "entity_manager.hpp"
#include "utils.hpp"
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

using ExportParameters = devices::ExportParameters;

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
    name << bus << "-" << std::hex << std::setw(4) << std::setfill('0') << address;
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

static int deleteDevice(const std::shared_ptr<ExportParameters>& params)
{
    std::ofstream deviceFile(params->destroy.path);
    if (!deviceFile.good())
    {
        std::cerr << "Error writing " << params->destroy.path << "\n";
        return -1;
    }
    deviceFile << params->destroy.data;
    deviceFile.close();
    return 0;
}

static int createDevice(const std::shared_ptr<ExportParameters>& params)
{
    std::ofstream deviceFile(params->construct.path);
    if (!deviceFile.good())
    {
        std::cerr << "Error writing " << params->construct.path << "\n";
        return -1;
    }
    deviceFile << params->construct.data;
    deviceFile.close();
    return 0;
}

static bool deviceIsCreated(const std::shared_ptr<ExportParameters>& params)
{
    std::error_code ec;
    // Ignore errors; anything but a clean 'true' is just fine as 'false'
    return std::filesystem::exists(params->presentPath, ec);
}

static int buildDevice(const std::shared_ptr<ExportParameters>& params,
                       const size_t retries = 5)
{
    if (!retries)
    {
        return -1;
    }

    // If it's already instantiated, there's nothing we need to do.
    if (deviceIsCreated(params))
    {
        return 0;
    }

    // Try to create the device
    createDevice(params);

    // If it didn't work, delete it and try again in 500ms
    if (!deviceIsCreated(params))
    {
        deleteDevice(params);

        std::shared_ptr<boost::asio::steady_timer> createTimer =
            std::make_shared<boost::asio::steady_timer>(io);
        createTimer->expires_after(std::chrono::milliseconds(500));
        createTimer->async_wait([createTimer, params, retries]
                                 (const boost::system::error_code& ec) {
            if (ec)
            {
                std::cerr << "Timer error: " << ec << "\n";
                return -2;
            }
            return buildDevice(params, retries - 1);
        });
    }

    return 0;
}

ExportParameters::ExportParameters(const devices::ExportTemplate& exportTemplate,
                                   const nlohmann::json& configuration)
{
    std::string parameters = exportTemplate.parameters;
    std::string busPath = exportTemplate.busPath;
    std::string addressStr = "";
    std::string name = "unknown";
    std::optional<uint64_t> bus = std::nullopt;
    std::optional<uint64_t> address = std::nullopt;

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
            addressStr = jsonToString(keyPair.value());
        }

        boost::replace_all(parameters, templateChar + keyPair.key(),
                           subsituteString);
        boost::replace_all(busPath, templateChar + keyPair.key(),
                           subsituteString);
    }

    std::filesystem::path ctorPath(busPath);
    ctorPath /= exportTemplate.add;

    std::filesystem::path dtorPath(busPath);
    dtorPath /= exportTemplate.remove;

    construct = OpParams{ctorPath, parameters};
    destroy = OpParams{dtorPath, addressStr};

    if (!bus.has_value() || !address.has_value())
    {
        presentPath = "";
    }
    else
    {
        presentPath = busPath;
        presentPath /= deviceDirName(bus.value(), address.value());
        if (exportTemplate.createsHWMon)
        {
            presentPath /= "hwmon";
        }
    }
}

ExportParameters* devices::getExportParameters(const nlohmann::json& config)
{
    auto findType = config.find("Type");
    if (findType == config.end() ||
        findType->type() != nlohmann::json::value_t::string)
    {
        return nullptr;
    }
    std::string type = findType.value().get<std::string>();
    auto device = devices::exportTemplates.find(type.c_str());
    if (device == devices::exportTemplates.end())
    {
        return nullptr;
    }

    return new ExportParameters(device->second, config);
}

void exportDevice(const std::string& type,
                  const devices::ExportTemplate& exportTemplate,
                  const nlohmann::json& configuration)
{
    ExportParameters params(exportTemplate, configuration);
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
    }

    int err = buildDevice(std::make_shared<ExportParameters>(params));

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
            if (isClientManaged(configuration))
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
