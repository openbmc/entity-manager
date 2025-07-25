// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2024 Hewlett Packard Enterprise

#include "machine_context.hpp"

#include <filesystem>
#include <fstream>

void MachineContext::populateFromDeviceTree()
{
    std::string nodeVal;
    std::ifstream vpdStream(nodeBasePath + std::string("model"));
    if (vpdStream && std::getline(vpdStream, nodeVal))
    {
        MachineContext::Asset::model(nodeVal);
        vpdStream.close();
    }

    vpdStream.open(nodeBasePath + std::string("serial-number"));
    if (vpdStream && std::getline(vpdStream, nodeVal))
    {
        MachineContext::Asset::serial_number(nodeVal);
        vpdStream.close();
    }
};

bool MachineContext::keyNodeExists()
{
    std::filesystem::path nodePath{nodeBasePath + std::string("model")};

    return std::filesystem::exists(nodePath);
};
