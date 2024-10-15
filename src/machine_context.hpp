/*
// Copyright (c) 2024 Hewlett Packard Enterprise
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

#pragma once

#include <sdbusplus/async.hpp>
#include <xyz/openbmc_project/Inventory/Decorator/Asset/aserver.hpp>

class MachineContext :
    public sdbusplus::aserver::xyz::openbmc_project::inventory::decorator::
        Asset<MachineContext>
{
  public:
    explicit MachineContext(sdbusplus::async::context& ctx, auto path) :
        sdbusplus::aserver::xyz::openbmc_project::inventory::decorator::Asset<
            MachineContext>(ctx, path) {};

    void populateFromDeviceTree();

    static bool keyNodeExists();

  private:
    static constexpr auto nodeBasePath = "/proc/device-tree/";
};
