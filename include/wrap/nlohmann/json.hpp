#pragma once

#include <nlohmann/json.hpp>

/*
 * Our meson.build is set up to correctly handle system / subproject
 * dependencies but this exposes a flaw in the valijson build specification when
 * it's provided as a subproject. We're unable to acquire the valijson headers
 * on their own as the only targets defined are examples. So we grab the example
 * headers, but this drags in the older, vendored nlohmann/json header. This
 * header uses a different header guard and results in type redefinitions. To
 * fix _that_, conditionally define the old header guard macro.
 */
#ifndef NLOHMANN_JSON_HPP
#define NLOHMANN_JSON_HPP
#endif
