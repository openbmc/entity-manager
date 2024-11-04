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

namespace sdbusplus::common::xyz::openbmc_project::configuration
{

struct GPIODeviceDetect
{
    static constexpr auto interface =
        "xyz.openbmc_project.Configuration.GPIODeviceDetect";

    using PropertiesVariant = sdbusplus::utility::dedup_variant_t<
        std::string, std::vector<std::string>, std::vector<uint64_t>>;
};

} // namespace sdbusplus::common::xyz::openbmc_project::configuration

namespace sdbusplus::message::details
{} // namespace sdbusplus::message::details
// NOLINTEND
