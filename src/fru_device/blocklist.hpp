// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stddef.h>

#include <flat_map>
#include <flat_set>
#include <optional>
#include <set>

using BusBlocklist =
    std::flat_map<size_t, std::optional<std::flat_set<size_t>>>;

struct Blocklist
{
    BusBlocklist buses;
    std::set<size_t> addresses;
};

// An unavailable optional file yields an empty blocklist; invalid contents
// yield std::nullopt.
std::optional<Blocklist> loadBlocklist(const char* path);
