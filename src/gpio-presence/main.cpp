/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024.
 * All rights reserved. SPDX-License-Identifier: Apache-2.0
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

    std::unique_ptr<gpio_presence::GPIOPresence> controller =
        std::make_unique<gpio_presence::GPIOPresence>(ctx);

    controller->setupBusName();

    ctx.spawn(controller->initialize());

    ctx.run();
}
