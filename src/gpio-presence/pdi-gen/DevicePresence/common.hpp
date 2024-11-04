#pragma once
// NOLINTBEGIN
#include <sdbusplus/exception.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/utility/dedup_variant.hpp>

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

namespace sdbusplus::common::xyz::openbmc_project::inventory::source
{

struct DevicePresence
{
    static constexpr auto interface =
        "xyz.openbmc_project.Inventory.Source.DevicePresence";

    using PropertiesVariant = sdbusplus::utility::dedup_variant_t<std::string>;
};

} // namespace sdbusplus::common::xyz::openbmc_project::inventory::source

namespace sdbusplus::message::details
{} // namespace sdbusplus::message::details
// NOLINTEND
