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

#include <string>
#include <iostream>
#include <regex>
#include <boost/process/child.hpp>
#include <experimental/filesystem>
#include <boost/container/flat_map.hpp>
#include <boost/container/flat_set.hpp>
#include <nlohmann/json.hpp>
#include <Overlay.hpp>
#include <Utils.hpp>

constexpr const char *DT_OVERLAY = "/usr/bin/dtoverlay";
constexpr const char *DTC = "/usr/bin/dtc";
constexpr const char *OUTPUT_DIR = "/tmp/overlays";
constexpr const char *TEMPLATE_DIR = "/usr/share/overlay_templates";
constexpr const char *TEMPLATE_CHAR = "$";
constexpr const char *PLATFORM = "aspeed,ast2500";
constexpr const char *I2C_DEVS_DIR = "/sys/class/i2c-dev";

// some drivers need to be unbind / bind to load device tree changes
static const boost::container::flat_map<std::string, std::string> FORCE_PROBES =
    {{"IntelFanConnector", "/sys/bus/platform/drivers/aspeed_pwm_tacho"}};

static const boost::container::flat_set<std::string> MUX_TYPES = {
    {"PCA9543Mux"}, {"PCA9545Mux"}};

std::regex ILLEGAL_NAME_REGEX("[^A-Za-z0-9_]");

void createOverlay(const std::string &templatePath,
                   const nlohmann::json &configuration);

void unloadAllOverlays(void)
{
    boost::process::child c(DT_OVERLAY, "-d", OUTPUT_DIR, "-R");
    c.wait();
}

// this is hopefully temporary, but some drivers can't detect changes
// without an unbind and bind so this function must exist for now
void forceProbe(const std::string &driver)
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

    std::experimental::filesystem::path pathObj(driver);
    for (auto &p : std::experimental::filesystem::directory_iterator(pathObj))
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
std::string jsonToString(const nlohmann::json &in)
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

// when muxes create new i2c devices, the symbols are not there to map the new
// i2c devices to the mux address. this looks up the device tree path and adds
// the new symbols so the new devices can be referenced via the phandle
void fixupSymbols(
    const std::vector<std::experimental::filesystem::path> &i2cDevsBefore)
{
    std::vector<std::experimental::filesystem::path> i2cDevsAfter;
    find_files(std::experimental::filesystem::path(I2C_DEVS_DIR),
               R"(i2c-\d+)", i2cDevsAfter, 0);

    for (const auto &dev : i2cDevsAfter)
    {
        if (std::find(i2cDevsBefore.begin(), i2cDevsBefore.end(), dev) !=
            i2cDevsBefore.end())
        {
            continue;
        }
        // removes everything before and including the '-' in /path/i2c-##
        std::string bus =
            std::regex_replace(dev.string(), std::regex("^.*-"), "");
        std::string devtreeRef = dev.string() + "/device/of_node";
        auto devtreePath = std::experimental::filesystem::path(devtreeRef);
        std::string symbolPath =
            std::experimental::filesystem::canonical(devtreePath);
        symbolPath =
            symbolPath.substr(sizeof("/sys/firmware/devicetree/base") - 1);
        nlohmann::json configuration = {{"path", symbolPath},
                                        {"Type", "Symbol"},
                                        {"bus", std::stoul(bus)},
                                        {"Name", "i2c" + bus}};
        createOverlay(TEMPLATE_DIR + std::string("/Symbol.template"),
                      configuration);
    }
}

void createOverlay(const std::string &templatePath,
                   const nlohmann::json &configuration)
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
        if (keyPair.key() == "bus" &&
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
        else
        {
            subsituteString = jsonToString(keyPair.value());
        }
        boost::replace_all(templateStr, TEMPLATE_CHAR + keyPair.key(),
                           subsituteString);
    }
    // todo: this is a lame way to fill in platform, but we only
    // care about ast2500 right now
    boost::replace_all(templateStr, TEMPLATE_CHAR + std::string("platform"),
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

    // this is for muxes, we need to store the diff of i2c devices before
    // and after scanning to load new symbols into device tree so that if we
    // later add devices to the "virtual" i2c device, we can match the phandle
    // to the correct mux
    std::vector<std::experimental::filesystem::path> i2cDevsBefore;
    auto findMux = MUX_TYPES.find(type);
    if (findMux != MUX_TYPES.end())
    {
        find_files(std::experimental::filesystem::path(I2C_DEVS_DIR),
                   R"(i2c-\d+)", i2cDevsBefore, 0);
    }

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
    if (findMux != MUX_TYPES.end())
    {
        fixupSymbols(i2cDevsBefore);
    }
}

bool loadOverlays(const nlohmann::json &systemConfiguration)
{

    std::vector<std::experimental::filesystem::path> paths;
    if (!find_files(std::experimental::filesystem::path(TEMPLATE_DIR),
                    R"(.*\.template)", paths, 0))
    {
        std::cerr << "Unable to find any tempate files in " << TEMPLATE_DIR
                  << "\n";
        return false;
    }

    std::experimental::filesystem::create_directory(OUTPUT_DIR);
    for (auto entity = systemConfiguration.begin();
         entity != systemConfiguration.end(); entity++)
    {
        auto findExposes = entity.value().find("exposes");
        if (findExposes == entity.value().end() ||
            findExposes->type() != nlohmann::json::value_t::array)
        {
            continue;
        }

        for (auto &configuration : *findExposes)
        {
            auto findStatus = configuration.find("status");
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
            std::string typeFile = type + std::string(".template");
            for (auto path : paths)
            {
                if (path.filename() != typeFile)
                {
                    continue;
                }
                createOverlay(path.string(), configuration);
                break;
            }
        }
    }

    return true;
}