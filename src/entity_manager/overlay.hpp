// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2018 Intel Corporation

#pragma once
#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>

void unloadAllOverlays();
bool loadOverlays(const nlohmann::json& systemConfiguration,
                  boost::asio::io_context& io);
