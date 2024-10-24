// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once

#include "../utils.hpp"
#include "devices.hpp"

#include <boost/asio/steady_timer.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <regex>

constexpr const char* outputDir = "/tmp/overlays";
constexpr const char* templateChar = "$";
constexpr const char* i2CDevsDir = "/sys/bus/i2c/devices";
constexpr const char* muxSymlinkDir = "/dev/i2c-mux";
const std::regex illegalNameRegex("[^A-Za-z0-9_]");

class DeviceConfig
{
  public:
    DeviceConfig() = delete;

    explicit DeviceConfig(const std::string& type,
                          const devices::ExportTemplate& exportTemplate,
                          const nlohmann::json& configuration) :
        type(type), parameters(exportTemplate.parameters),
        busPath(exportTemplate.busPath), constructor(exportTemplate.add),
        destructor(exportTemplate.remove),
        hasHWMonDir(exportTemplate.hasHWMonDir)
    {
        for (auto keyPair = configuration.begin();
             keyPair != configuration.end(); keyPair++)
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
            replaceAll(parameters, templateChar + keyPair.key(),
                       subsituteString);
            replaceAll(busPath, templateChar + keyPair.key(), subsituteString);
        }
    };

    // helper function to make json types into string
    static std::string jsonToString(const nlohmann::json& in)
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

    bool buildDevice(boost::asio::io_context& io, const size_t retries = 5);
    bool isMux();
    std::vector<DeviceConfig> linkMux(nlohmann::json& systemConfiguration);

  private:
    bool deviceIsCreated();
    bool createDevice();
    bool deleteDevice();

    std::string type;
    std::string parameters;
    std::string busPath;
    std::string constructor;
    std::string destructor;
    devices::createsHWMon hasHWMonDir;
    std::string name = "unknown";
    std::optional<uint64_t> bus;
    std::optional<uint64_t> address;
    std::vector<std::string> channels;
};

void unloadAllOverlays();
bool loadOverlays(const nlohmann::json& systemConfiguration,
                  nlohmann::json& currConfiguration,
                  boost::asio::io_context& io);
