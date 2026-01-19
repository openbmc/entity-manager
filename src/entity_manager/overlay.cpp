// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#include "overlay.hpp"

#include "../utils.hpp"
#include "devices.hpp"
#include "utils.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/container/flat_set.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/lg2.hpp>

#include <filesystem>
#include <flat_map>
#include <fstream>
#include <iomanip>
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
        std::ranges::replace(array, ',', ' ');
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

void linkMux(std::string_view muxName, uint64_t busIndex, uint64_t address,
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
            lg2::error("{PATH} for mux channel {CHANNEL} doesn't exist!",
                       "PATH", channelPath.string(), "CHANNEL", channelName);
            continue;
        }
        std::filesystem::path bus = std::filesystem::read_symlink(channelPath);

        std::filesystem::path fp("/dev" / bus.filename());
        std::filesystem::path link(linkDir / channelName);

        std::filesystem::create_symlink(fp, link, ec);
        if (ec)
        {
            lg2::error("Failure creating symlink for {PATH} to {LINK}", "PATH",
                       fp.string(), "LINK", link.string());
        }
    }
}

static int deleteDevice(std::string_view busPath, uint64_t address,
                        std::string_view destructor)
{
    std::filesystem::path deviceDestructor(busPath);
    deviceDestructor /= destructor;
    std::ofstream deviceFile(deviceDestructor);
    if (!deviceFile.good())
    {
        lg2::error("Error writing {PATH}", "PATH", deviceDestructor.string());
        return -1;
    }
    deviceFile << std::to_string(address);
    deviceFile.close();
    return 0;
}

static int createDevice(std::string_view busPath, std::string_view parameters,
                        std::string_view constructor)
{
    std::filesystem::path deviceConstructor(busPath);
    deviceConstructor /= constructor;
    std::ofstream deviceFile(deviceConstructor);
    if (!deviceFile.good())
    {
        lg2::error("Error writing {PATH}", "PATH", deviceConstructor.string());
        return -1;
    }
    deviceFile << parameters;
    deviceFile.close();

    return 0;
}

static bool deviceIsCreated(std::string_view busPath, uint64_t bus,
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

static int buildDevice(
    std::string_view name, std::string_view busPath,
    std::string_view parameters, uint64_t bus, uint64_t address,
    std::string_view constructor, std::string_view destructor,
    const devices::createsHWMon hasHWMonDir,
    std::vector<std::string> channelNames, boost::asio::io_context& io,
    const size_t retries = 5)
{
    if (retries == 0U)
    {
        return -1;
    }

    lg2::debug("try to build device {NAME} at bus={BUS}, address={ADDR}",
               "NAME", name, "BUS", bus, "ADDR", address);

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
                 channelNames(std::move(channelNames)), retries,
                 &io](const boost::system::error_code& ec) mutable {
                    if (ec)
                    {
                        lg2::error("Timer error: {ERR}", "ERR", ec.message());
                        return -2;
                    }
                    return buildDevice(name, busPath, parameters, bus, address,
                                       constructor, destructor, hasHWMonDir,
                                       std::move(channelNames), io,
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

void exportDevice(const devices::ExportTemplate& exportTemplate,
                  const nlohmann::json& configuration,
                  boost::asio::io_context& io)
{
    std::string_view type = exportTemplate.type;
    std::string parameters(exportTemplate.parameters);
    std::string busPath(exportTemplate.busPath);
    std::string_view constructor = exportTemplate.add;
    std::string_view destructor = exportTemplate.remove;
    devices::createsHWMon hasHWMonDir = exportTemplate.hasHWMonDir;
    std::string name = "unknown";
    std::optional<uint64_t> bus;
    std::optional<uint64_t> address;
    std::vector<std::string> channels;

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

        if (keyPair.key() == "Bus" &&
            (keyPair.value().type() ==
                 nlohmann::json::value_t::number_integer ||
             keyPair.value().type() ==
                 nlohmann::json::value_t::number_unsigned))
        {
            bus = keyPair.value().get<uint64_t>();
        }
        else if (keyPair.key() == "Address" &&
                 (keyPair.value().type() ==
                      nlohmann::json::value_t::number_integer ||
                  keyPair.value().type() ==
                      nlohmann::json::value_t::number_unsigned))
        {
            address = keyPair.value().get<uint64_t>();
        }
        else if (keyPair.key() == "ChannelNames" && type.ends_with("Mux") &&
                 (keyPair.value().type() == nlohmann::json::value_t::array))
        {
            channels = keyPair.value().get<std::vector<std::string>>();
        }
        replaceAll(parameters, templateChar + keyPair.key(), subsituteString);
        replaceAll(busPath, templateChar + keyPair.key(), subsituteString);
    }

    if (!bus || !address)
    {
        createDevice(busPath, parameters, constructor);
        return;
    }

    buildDevice(name, busPath, parameters, *bus, *address, constructor,
                destructor, hasHWMonDir, std::move(channels), io);
}

bool loadOverlays(const SystemConfiguration& systemConfiguration,
                  boost::asio::io_context& io)
{
    lg2::debug("start loading device overlays");

    std::filesystem::create_directory(outputDir);
    for (const auto& [name, entity] : systemConfiguration)
    {
        for (const auto& configuration : entity.exposesRecords)
        {
            auto findStatus = configuration.find("Status");
            // status missing is assumed to be 'okay'
            if (findStatus != configuration.end() &&
                configuration.at("Status") == "disabled")
            {
                continue;
            }
            if (!configuration.contains("Type"))
            {
                continue;
            }
            if (configuration.at("Type").type() !=
                nlohmann::json::value_t::string)
            {
                continue;
            }

            const std::string& type =
                configuration.at("Type").get<std::string>();
            const auto* device = std::ranges::find_if(
                devices::exportTemplates,
                [&type](const auto& tmp) { return tmp.type == type; });
            if (device != devices::exportTemplates.end())
            {
                exportDevice(*device, configuration, io);
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

    lg2::debug("finish loading device overlays");

    return true;
}
