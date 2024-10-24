// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "overlay.hpp"

#include "utils.hpp"

#include <fstream>

constexpr const bool debug = false;

static std::string deviceDirName(uint64_t bus, uint64_t address)
{
    std::ostringstream name;
    name << bus << "-" << std::hex << std::setw(4) << std::setfill('0')
         << address;
    return name.str();
}

bool loadOverlays(const nlohmann::json& systemConfiguration,
                  nlohmann::json& currConfiguration,
                  boost::asio::io_context& io)
{
    std::vector<DeviceConfig> deviceConfigurations;
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
                deviceConfigurations.emplace_back(type, device->second,
                                                  configuration);
                continue;
            }

            // Because many devices are intentionally not exportable,
            // this error message is not printed in all situations.
            // If wondering why your device not appearing, add your type to
            // the exportTemplates array in the devices.hpp file.
            if constexpr (debug)
            {
                std::cerr << "Device type " << type
                          << " not found in export map allowlist\n";
            }
        }
    }

    std::vector<DeviceConfig> newMuxConfigs;
    for (auto& config : deviceConfigurations)
    {
        if (config.buildDevice(io))
        {
            if (config.isMux())
            {
                std::vector<DeviceConfig> linkedConfigs =
                    config.linkMux(currConfiguration);
                for (const auto& linkedConfig : linkedConfigs)
                {
                    newMuxConfigs.emplace_back(linkedConfig);
                }
            }
        }
    }
    while (!newMuxConfigs.empty())
    {
        std::vector<DeviceConfig> muxConfigs(newMuxConfigs);
        newMuxConfigs.clear();
        for (auto& newMux : muxConfigs)
        {
            if (newMux.buildDevice(io))
            {
                std::vector<DeviceConfig> linkedConfigs =
                    newMux.linkMux(currConfiguration);
                for (const auto& linkedConfig : linkedConfigs)
                {
                    newMuxConfigs.emplace_back(linkedConfig);
                }
            }
        }
    }
    return true;
}

bool DeviceConfig::deviceIsCreated()
{
    std::filesystem::path dirPath = busPath;
    if (!bus.has_value() || !address.has_value())
    {
        return false;
    }
    dirPath /= deviceDirName(*bus, *address);
    if (hasHWMonDir == devices::createsHWMon::hasHWMonDir)
    {
        dirPath /= "hwmon";
    }

    std::error_code ec;
    // Ignore errors; anything but a clean 'true' is just fine as 'false'
    return std::filesystem::exists(dirPath, ec);
}

bool DeviceConfig::createDevice()
{
    std::filesystem::path deviceConstructor(busPath);
    deviceConstructor /= constructor;
    std::ofstream deviceFile(deviceConstructor);
    if (!deviceFile.good())
    {
        std::cerr << "Error writing " << deviceConstructor << "\n";
        return false;
    }
    deviceFile << parameters;
    deviceFile.close();
    return true;
}

bool DeviceConfig::deleteDevice()
{
    std::filesystem::path deviceDestructor(busPath);
    deviceDestructor /= destructor;
    std::ofstream deviceFile(deviceDestructor);
    if (!deviceFile.good() || !address.has_value())
    {
        std::cerr << "Error writing " << deviceDestructor << "\n";
        return false;
    }
    deviceFile << std::to_string(*address);
    deviceFile.close();
    return true;
}

bool DeviceConfig::buildDevice(boost::asio::io_context& io,
                               const size_t retries)
{
    if (retries == 0U)
    {
        return false;
    }
    if (!bus.has_value() || !address.has_value())
    {
        return false;
    }

    // If it's already instantiated, we don't need to create it again.
    if (!deviceIsCreated())
    {
        // If it didn't work, delete it and try again in 500ms
        if (!createDevice())
        {
            deleteDevice();

            std::shared_ptr<boost::asio::steady_timer> createTimer =
                std::make_shared<boost::asio::steady_timer>(io);
            createTimer->expires_after(std::chrono::milliseconds(500));
            createTimer->async_wait(
                [createTimer, this, retries,
                 &io](const boost::system::error_code& ec) mutable {
                    if (ec)
                    {
                        std::cerr << "Timer error: " << ec << "\n";
                        return false;
                    }
                    return buildDevice(io, retries - 1);
                });
            return false;
        }
    }

    return true;
}

bool DeviceConfig::isMux()
{
    return (type.ends_with("Mux") && !channels.empty());
}

std::vector<DeviceConfig> DeviceConfig::linkMux(
    nlohmann::json& systemConfiguration)
{
    std::error_code ec;
    std::filesystem::path muxSymlinkDirPath(muxSymlinkDir);
    std::filesystem::create_directory(muxSymlinkDirPath, ec);
    // ignore error codes here if the directory already exists
    ec.clear();
    std::filesystem::path linkDir = muxSymlinkDirPath / name;
    std::filesystem::create_directory(linkDir, ec);

    std::filesystem::path devDir(i2CDevsDir);
    if (!bus.has_value() || !address.has_value())
    {
        return {};
    }
    devDir /= deviceDirName(*bus, *address);

    std::vector<DeviceConfig> newMuxConfigs;
    for (std::size_t channelIndex = 0; channelIndex < channels.size();
         channelIndex++)
    {
        const std::string& channelName = channels[channelIndex];
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
        std::filesystem::path busFile =
            std::filesystem::read_symlink(channelPath);

        std::filesystem::path fp("/dev" / busFile.filename());
        std::filesystem::path link(linkDir / channelName);
        uint64_t busNumber = busStrToInt(busFile.filename().string());

        std::filesystem::create_symlink(fp, link, ec);
        if (ec)
        {
            if (ec != std::make_error_code(std::errc::file_exists))
            {
                std::cerr << "Failure creating symlink for " << fp << " to "
                          << link << "\n";
                std::cerr << ec.message() << std::endl;
                continue;
            }

            auto linkedBus = std::filesystem::read_symlink(link);
            uint64_t linkedBusNumber =
                busStrToInt(linkedBus.filename().string());
            if (busNumber != linkedBusNumber)
            {
                std::cerr << "New busNumber attached to ChannelName"
                          << std::endl;
                std::filesystem::remove(link);
                std::filesystem::create_symlink(fp, link, ec);
                if (ec)
                {
                    std::cerr << "Failure creating symlink for " << fp << " to "
                              << link << "\n";
                    std::cerr << ec.message() << std::endl;
                    continue;
                }
            }
        }
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
                auto findMuxChannel = configuration.find("MuxChannel");
                auto findName = configuration.find("Name");
                if (findMuxChannel == configuration.end())
                {
                    continue;
                }
                if (findMuxChannel->type() != nlohmann::json::value_t::object)
                {
                    std::cout << "Incomplete MuxChannel for " << *findName
                              << std::endl;
                    continue;
                }
                auto findMuxName = findMuxChannel.value().find("MuxName");
                auto findChName = findMuxChannel.value().find("ChannelName");
                if (findMuxName == findMuxChannel.value().end() ||
                    findChName == findMuxChannel.value().end() ||
                    *findMuxName != name || *findChName != channelName)
                {
                    continue;
                }
                if constexpr (debug)
                {
                    std::cout << "Applying " << *findName
                              << " to bus: " << busNumber << std::endl;
                }
                configuration["Bus"] = busNumber;
                auto findType = configuration.find("Type");
                std::string cfgType = findType.value().get<std::string>();
                if (cfgType.ends_with("Mux"))
                {
                    auto device =
                        devices::exportTemplates.find(cfgType.c_str());
                    if (device != devices::exportTemplates.end())
                    {
                        newMuxConfigs.emplace_back(cfgType, device->second,
                                                   configuration);
                    }
                }
            }
        }
    }
    return newMuxConfigs;
}
