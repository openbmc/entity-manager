#include "../dbus_util.hpp"
#include "../fru_device/fru_utils.hpp"
#include "neard_dbus.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <phosphor-logging/lg2.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <algorithm>
#include <cstdint>
#include <flat_map>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

static constexpr int maxRetry = 10;

struct Context
{
    boost::asio::io_context io;

    std::shared_ptr<sdbusplus::asio::connection> conn;
    std::unique_ptr<sdbusplus::asio::object_server> server;
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface;

    boost::asio::steady_timer timer;

    std::string recordPath;
    int retryCount = 0;

    Context() : timer(io) {}
};

using FruMap = std::flat_map<std::string, std::string, std::less<>>;

static std::optional<FruMap> decodeNfcFru(const std::vector<uint8_t>& payload)
{
    FruMap formattedFRU;
    auto rc = formatIPMIFRU(payload, formattedFRU);

    if (rc == resCodes::resErr)
    {
        lg2::error("formatIPMIFRU failed");
        return std::nullopt;
    }

    return formattedFRU;
}

static void addFruObjectToDbus(Context& ctx, const FruMap& formattedFRU)
{
    if (ctx.iface)
    {
        ctx.server->remove_interface(ctx.iface);
        ctx.iface.reset();
    }

    std::string productName = "/xyz/openbmc_project/FruDevice/";

    auto productNameFind = formattedFRU.find("BOARD_PRODUCT_NAME");
    if (productNameFind == formattedFRU.end() ||
        productNameFind->second.empty())
    {
        productNameFind = formattedFRU.find("PRODUCT_PRODUCT_NAME");
    }

    if (productNameFind != formattedFRU.end() &&
        !productNameFind->second.empty())
    {
        productName +=
            dbus_util::sanitizeForDBusPathSegment(productNameFind->second);
    }
    else
    {
        productName += "UNKNOWN";
    }

    ctx.iface =
        ctx.server->add_interface(productName, "xyz.openbmc_project.FruDevice");

    for (const auto& [key, value] : formattedFRU)
    {
        ctx.iface->register_property(key, value);
    }

    ctx.iface->initialize();
}

static void updateFruProperty(Context& ctx)
{
    auto& bus = *ctx.conn;

    try
    {
        auto payload = getMimePayload(bus, ctx.recordPath);
        auto fruOpt = decodeNfcFru(payload);
        if (!fruOpt)
        {
            lg2::warning("No valid FRU decoded from NFC payload");
            return;
        }

        addFruObjectToDbus(ctx, *fruOpt);
    }
    catch (const std::exception& e)
    {
        lg2::error("updateFruProperty failed: {ERR}", "ERR", e);
    }
}

static void waitTagRecord(Context& ctx)
{
    auto& bus = *ctx.conn;

    auto recordPathOpt = findFruRecordPath(bus);
    if (!recordPathOpt)
    {
        if (++ctx.retryCount >= maxRetry)
        {
            lg2::error("NFC timeout: no FRU tag found");
            return;
        }

        ctx.timer.expires_after(std::chrono::milliseconds(200));
        ctx.timer.async_wait([&ctx](const boost::system::error_code& ec) {
            if (!ec)
                waitTagRecord(ctx);
        });

        return;
    }

    ctx.recordPath = *recordPathOpt;
    ctx.retryCount = 0;
    updateFruProperty(ctx);
}

int main()
{
    Context ctx;

    ctx.conn = std::make_shared<sdbusplus::asio::connection>(ctx.io);
    ctx.server = std::make_unique<sdbusplus::asio::object_server>(ctx.conn);

    ctx.conn->request_name("xyz.openbmc_project.NfcFruDevice");

    ctx.timer.expires_after(std::chrono::milliseconds(200));
    ctx.timer.async_wait([&ctx](const boost::system::error_code& ec) {
        if (!ec)
            waitTagRecord(ctx);
    });

    ctx.io.run();
    return 0;
}
