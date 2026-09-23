// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2024 Hewlett Packard Enterprise

#pragma once

#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/aserver.hpp>

class MachineContext :
    public sdbusplus::aserver::xyz::openbmc_project::inventory::decorator::
        Asset<MachineContext>
{
  public:
    MachineContext(sdbusplus::async::context& ctx, auto path) :
        sdbusplus::aserver::xyz::openbmc_project::inventory::decorator::Asset<
            MachineContext>(ctx, path, signal_action::defer_emit)
    {
        populateFromDeviceTree();
        emit_object_added();
    };

    void populateFromDeviceTree();

    static bool keyNodeExists();

  private:
    static constexpr auto nodeBasePath = "/proc/device-tree/";
};
