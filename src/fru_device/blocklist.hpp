// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stddef.h>

#include <flat_map>
#include <flat_set>
#include <optional>
#include <set>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern std::flat_map<size_t, std::optional<std::flat_set<size_t>>> busBlocklist;

std::set<size_t> loadBlocklist(const char* path);
