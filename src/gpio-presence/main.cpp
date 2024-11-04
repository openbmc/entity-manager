/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024.
 * All rights reserved. SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gpio_presence.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>

#include <memory>

using namespace gpio_presence;

int main()
{
    lg2::debug("starting GPIO Presence Sensor");

    sdbusplus::async::context ctx;

    ConfigProvider configProvider(ctx);

    std::unique_ptr<gpio_presence::GPIOPresence> controller =
        std::make_unique<gpio_presence::GPIOPresence>(ctx, configProvider);

    controller->setupBusName();

    ctx.spawn(configProvider.initialize(ctx, controller));

    ctx.run();
}
