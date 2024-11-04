/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024.
 * All rights reserved. SPDX-License-Identifier: Apache-2.0
 */

#include "gpio_presence_manager.hpp"

#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/async/context.hpp>

#include <memory>

using namespace gpio_presence;

int main()
{
    lg2::debug("starting GPIO Presence Sensor");

    sdbusplus::async::context ctx;

    gpio_presence::GPIOPresenceManager controller(ctx);

    controller.setupBusName();

    ctx.spawn(controller.initialize());

    ctx.run();
}
