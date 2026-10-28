// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2018 Intel Corporation

#pragma once
#include "system_configuration.hpp"

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>

void unloadAllOverlays();
bool loadOverlays(const SystemConfiguration& systemConfiguration,
                  boost::asio::io_context& io);
