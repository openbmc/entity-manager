// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once

#include "fru_utils.hpp"
#include "nfc.hpp"

#include <sdbusplus/asio/object_server.hpp>

#include <cstdint>
#include <set>
#include <vector>

// Runtime state shared by the fru-device scan/publish paths. All members
// share the same lifetime (owned by main()) and are always passed together.
struct FruDetails
{
    DBusIntfMap dbusInterfaceMap;
    size_t unknownBusObjectCount = 0;
    bool powerIsOn = false;
    std::set<size_t> addressBlocklist;

    NfcFruState nfcFruState;
};

void addFruObjectToDbus(std::vector<uint8_t>& device, FruDetails& fruDetails,
                        uint32_t bus, uint32_t address,
                        sdbusplus::asio::object_server& objServer);
