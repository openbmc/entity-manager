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

#include "filesystem.hpp"

#include <Overlay.hpp>
#include <Utils.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/process/child.hpp>
#include <devices.hpp>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>

constexpr const char* DT_OVERLAY = "/usr/bin/dtoverlay";
constexpr const char* DTC = "/usr/bin/dtc";
constexpr const char* OUTPUT_DIR = "/tmp/overlays";
constexpr const char* TEMPLATE_DIR = PACKAGE_DIR "overlay_templates";
constexpr const char* TEMPLATE_CHAR = "$";
constexpr const char* HEX_FORMAT_STR = "0x";
constexpr const char* PLATFORM = "aspeed,ast2500";
constexpr const char* I2C_DEVS_DIR = "/sys/bus/i2c/devices";
constexpr const char *MUX_SYMLINK_DIR = "/dev/i2c-mux";

// some drivers need to be unbind / bind to load device tree changes
static const boost::container::flat_map<std::string, std::string> FORCE_PROBES =
    {{"IntelFanConnector", "/sys/bus/platform/drivers/aspeed_pwm_tacho"}};

std::regex ILLEGAL_NAME_REGEX("[^A-Za-z0-9_]");

void createOverlay(const std::string& templatePath,
                   const nlohmann::json& configuration);

void unloadAllOverlays(void)
{
    boost::process::child c(DT_OVERLAY, "-d", OUTPUT_DIR, "-R");
    c.wait();
}

// this is hopefully temporary, but some drivers can't detect changes
// without an unbind and bind so this function must exist for now
void forceProbe(const std::string& driver)
{
    std::ofstream driverUnbind(driver + "/unbind");
    std::ofstream driverBind(driver + "/bind");
    if (!driverUnbind.is_open())
    {
        std::cerr << "force probe error opening " << driver << "\n";
        return;
    }
    if (!driverBind.is_open())
    {
        driverUnbind.close();
        std::cerr << "force probe error opening " << driver << "\n";
        return;
    }

    std::filesystem::path pathObj(driver);
    for (auto& p : std::filesystem::directory_iterator(pathObj))
    {
        // symlinks are object names
        if (is_symlink(p))
        {
            std::string driverName = p.path().filename();
            driverUnbind << driverName;
            driverBind << driverName;
            break;
        }
    }
    driverUnbind.close();
    driverBind.close();
}

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

void linkMux(const std::string& muxName, size_t bus, size_t address,
             const nlohmann::json::array_t& channelNames)
{
    // create directory first time
    static bool createDir = false;
    std::error_code ec;
    if (!createDir)
    {
        std::filesystem::create_directory(MUX_SYMLINK_DIR, ec);
        createDir = true;
    }
    std::ostringstream hex;
    hex << std::hex << std::setfill('0') << std::setw(4) << address;
    const std::string& addressHex = hex.str();

    std::filesystem::path devDir(I2C_DEVS_DIR);
    devDir /= std::to_string(bus) + "-" + addressHex;

    size_t channel = 0;
    std::string channelName;
    std::filesystem::path channelPath = devDir / "channel-0";
    while (is_symlink(channelPath))
    {
        if (channel > channelNames.size())
        {
            channelName = std::to_string(channel);
        }
        else
        {
            const std::string* ptr =
                channelNames[channel].get_ptr<const std::string*>();
            if (ptr == nullptr)
            {
                channelName = channelNames[channel].dump();
            }
            else
            {
                channelName = *ptr;
            }
        }
        // if configuration has name empty, don't put it in dev
        if (channelName.empty())
        {
            continue;
        }

        std::filesystem::path bus = std::filesystem::read_symlink(channelPath);
        const std::string& busName = bus.filename();

        std::string linkDir = MUX_SYMLINK_DIR + std::string("/") + muxName;
        if (channel == 0)
        {
            std::filesystem::create_directory(linkDir, ec);
        }
        std::filesystem::create_symlink(
            "/dev/" + busName, linkDir + std::string("/") + channelName, ec);

        if (ec)
        {
            std::cerr << "Failure creating symlink for " << busName << "\n";
            return;
        }

        channel++;
        channelPath = devDir / ("channel-" + std::to_string(channel));
    }
    if (channel == 0)
    {
        std::cerr << "Mux missing channels " << devDir << "\n";
    }
}

void exportDevice(const std::string& type,
                  const devices::ExportTemplate& exportTemplate,
                  const nlohmann::json& configuration)
{

    std::string parameters = exportTemplate.parameters;
    std::string device = exportTemplate.device;
    std::string name = "unknown";
    const uint64_t* bus = nullptr;
    const uint64_t* address = nullptr;
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
            bus = keyPair.value().get_ptr<const uint64_t*>();
        }
        else if (keyPair.key() == "Address")
        {
            address = keyPair.value().get_ptr<const uint64_t*>();
        }
        else if (keyPair.key() == "ChannelNames")
        {
            channels =
                keyPair.value().get_ptr<const nlohmann::json::array_t*>();
        }
        boost::replace_all(parameters, TEMPLATE_CHAR + keyPair.key(),
                           subsituteString);
        boost::replace_all(device, TEMPLATE_CHAR + keyPair.key(),
                           subsituteString);
    }

    // if we found bus and address we can attempt to prevent errors
    if (bus != nullptr && address != nullptr)
    {
        std::ostringstream hex;
        hex << std::hex << *address;
        const std::string& addressHex = hex.str();
        std::string busStr = std::to_string(*bus);

        std::filesystem::path devicePath(device);
        const std::string& dir = devicePath.parent_path().string();
        for (const auto& path : std::filesystem::directory_iterator(dir))
        {
            if (!std::filesystem::is_directory(path))
            {
                continue;
            }

            const std::string& directoryName = path.path().filename();
            if (boost::starts_with(directoryName, busStr) &&
                boost::ends_with(directoryName, addressHex))
            {
                return; // already exported
            }
        }
    }

    std::ofstream deviceFile(device);
    if (!deviceFile.good())
    {
        std::cerr << "Error writing " << device << "\n";
        return;
    }
    deviceFile << parameters;
    deviceFile.close();
    if (boost::ends_with(type, "Mux") && bus && address && channels)
    {
        linkMux(name, *bus, *address, *channels);
    }
}

// this is now deprecated
void createOverlay(const std::string& templatePath,
                   const nlohmann::json& configuration)
{
    std::ifstream templateFile(templatePath);

    if (!templateFile.is_open())
    {
        std::cerr << "createOverlay error opening " << templatePath << "\n";
        return;
    }
    std::stringstream buff;
    buff << templateFile.rdbuf();
    std::string templateStr = buff.str();
    std::string name = "unknown";
    std::string type = configuration["Type"];
    for (auto keyPair = configuration.begin(); keyPair != configuration.end();
         keyPair++)
    {
        std::string subsituteString;

        // device tree symbols are in decimal
        if (keyPair.key() == "Bus" &&
            keyPair.value().type() == nlohmann::json::value_t::string)
        {
            unsigned int dec =
                std::stoul(keyPair.value().get<std::string>(), nullptr, 16);
            subsituteString = std::to_string(dec);
        }
        else if (keyPair.key() == "Name" &&
                 keyPair.value().type() == nlohmann::json::value_t::string)
        {
            subsituteString = std::regex_replace(
                keyPair.value().get<std::string>(), ILLEGAL_NAME_REGEX, "_");
            name = subsituteString;
        }
        else if (keyPair.key() == "Address")
        {
            if (keyPair.value().type() == nlohmann::json::value_t::string)
            {
                subsituteString = keyPair.value().get<std::string>();
                subsituteString.erase(
                    0, subsituteString.find_first_not_of(HEX_FORMAT_STR));
            }
            else
            {
                std::ostringstream hex;
                hex << std::hex << keyPair.value().get<unsigned int>();
                subsituteString = hex.str();
            }
        }
        else
        {
            subsituteString = jsonToString(keyPair.value());
        }
        boost::replace_all(templateStr, TEMPLATE_CHAR + keyPair.key(),
                           subsituteString);
    }
    // todo: this is a lame way to fill in platform, but we only
    // care about ast2500 right now
    boost::replace_all(templateStr, TEMPLATE_CHAR + std::string("Platform"),
                       PLATFORM);
    std::string dtsFilename =
        std::string(OUTPUT_DIR) + "/" + name + "_" + type + ".dts";
    std::string dtboFilename =
        std::string(OUTPUT_DIR) + "/" + name + "_" + type + ".dtbo";

    std::ofstream out(dtsFilename);
    if (!out.is_open())
    {
        std::cerr << "createOverlay error opening " << dtsFilename << "\n";
        return;
    }
    out << templateStr;
    out.close();

    // compile dtbo and load overlay
    boost::process::child c1(DTC, "-@", "-q", "-I", "dts", "-O", "dtb", "-o",
                             dtboFilename, dtsFilename);
    c1.wait();
    if (c1.exit_code())
    {
        std::cerr << "DTC error with file " << dtsFilename << "\n";
        return;
    }
    boost::process::child c2(DT_OVERLAY, "-d", OUTPUT_DIR, name + "_" + type);
    c2.wait();
    if (c2.exit_code())
    {
        std::cerr << "DTOverlay error with file " << dtboFilename << "\n";
        return;
    }
    auto findForceProbe = FORCE_PROBES.find(type);
    if (findForceProbe != FORCE_PROBES.end())
    {
        forceProbe(findForceProbe->second);
    }
}

bool loadOverlays(const nlohmann::json& systemConfiguration)
{

    std::vector<std::filesystem::path> paths;
    if (!findFiles(std::filesystem::path(TEMPLATE_DIR),
                   R"(.*\.template)", paths))
    {
        std::cerr << "Unable to find any tempate files in " << TEMPLATE_DIR
                  << "\n";
        return false;
    }

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
#if OVERLAYS

            std::string typeFile = type + std::string(".template");
            for (const auto& path : paths)
            {
                if (path.filename() != typeFile)
                {
                    continue;
                }
                createOverlay(path.string(), configuration);
                break;
            }
#endif
            auto device = devices::exportTemplates.find(type.c_str());
            if (device != devices::exportTemplates.end())
            {
                exportDevice(type, device->second, configuration);
            }
        }
    }

    return true;
}
