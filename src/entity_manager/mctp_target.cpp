// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: Copyright 2026 Intel Corporation

#include "mctp_target.hpp"

#include "../dbus_util.hpp"
#include "config_pointer.hpp"
#include "utils.hpp"

#include <boost/asio/post.hpp>
#include <phosphor-logging/lg2.hpp>
#include <xyz/openbmc_project/Common/error.hpp>
#include <xyz/openbmc_project/Configuration/MCTPTarget/common.hpp>

#include <format>
#include <optional>
#include <string>

namespace mctp_target
{

using sdbusplus::xyz::openbmc_project::Common::Error::InternalFailure;
using sdbusplus::xyz::openbmc_project::Common::Error::InvalidArgument;
using sdbusplus::xyz::openbmc_project::Common::Error::ResourceNotFound;

static constexpr const char* mctpI2CTargetType = "MCTPI2CTarget";

MCTPTarget::MCTPTarget(dbus_interface::EMDBusInterface& dbusInterface) :
    dbusInterface(dbusInterface)
{}

sdbusplus::object_path MCTPTarget::addI2CTarget(
    const std::string& name, uint64_t bus, uint8_t address,
    nlohmann::json& systemConfiguration, const std::string& boardId,
    const sdbusplus::object_path& path, const std::string& board)
{
    if (name.empty())
    {
        throw InvalidArgument();
    }
    if (address > 0x7f)
    {
        throw InvalidArgument();
    }

    nlohmann::json& base = systemConfiguration[boardId];
    if (!base.contains("Exposes"))
    {
        throw InvalidArgument();
    }

    nlohmann::json::object_t newData{};
    newData["Name"] = name;
    newData["Type"] = mctpI2CTargetType;
    newData["Bus"] = bus;
    // The MCTPI2CTarget schema requires Address as a string.
    newData["Address"] = std::format("0x{:02x}", address);

    dbusInterface.addObjectJson(newData, systemConfiguration, boardId, path,
                                board);

    return path / dbus_util::sanitizeForDBusPathSegment(name);
}

void MCTPTarget::removeI2CTarget(
    const std::string& name, nlohmann::json& systemConfiguration,
    const std::string& boardId, const sdbusplus::object_path& path,
    const std::string& board)
{
    nlohmann::json& base = systemConfiguration[boardId];
    auto findExposes = base.find("Exposes");
    if (findExposes == base.end() || !findExposes->is_array())
    {
        throw ResourceNotFound();
    }

    std::optional<size_t> index;
    for (size_t i = 0; i < findExposes->size(); i++)
    {
        const nlohmann::json& expose = (*findExposes)[i];
        if (!expose.is_object())
        {
            continue;
        }
        auto findName = expose.find("Name");
        auto findType = expose.find("Type");
        if (findName != expose.end() && findType != expose.end() &&
            *findName == name && *findType == mctpI2CTargetType)
        {
            index = i;
            break;
        }
    }
    if (!index)
    {
        throw ResourceNotFound();
    }

    if (!ConfigPointer(boardId, *index).write(nullptr, systemConfiguration))
    {
        throw InternalFailure();
    }

    const std::string objPath =
        (path / dbus_util::sanitizeForDBusPathSegment(name)).str;
    const std::string ifaceName =
        std::string("xyz.openbmc_project.Configuration.") + mctpI2CTargetType;

    auto findBoard = dbusInterface.inventory.find(board);
    if (findBoard != dbusInterface.inventory.end())
    {
        for (const auto& weakIface : findBoard->second)
        {
            std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
                weakIface.lock();
            if (!iface || iface->get_object_path() != objPath)
            {
                continue;
            }
            const std::string intf = iface->get_interface_name();
            if (intf == ifaceName || intf.starts_with(ifaceName + "."))
            {
                boost::asio::post(dbusInterface.io, [this, iface]() mutable {
                    dbusInterface.objServer.remove_interface(iface);
                });
            }
        }
    }

    if (!dbusInterface.configCache.writeJsonFiles(systemConfiguration))
    {
        lg2::error("error setting json file");
        throw InternalFailure();
    }
}

void MCTPTarget::createMethods(
    const std::string& boardId, const sdbusplus::object_path& path,
    nlohmann::json& systemConfiguration, const std::string& board)
{
    std::shared_ptr<sdbusplus::asio::dbus_interface> iface =
        dbusInterface.createInterface(path,
                                      sdbusplus::common::xyz::openbmc_project::
                                          configuration::MCTPTarget::interface,
                                      board);

    iface->register_method(
        "AddMCTPI2CTarget",
        [&systemConfiguration, boardId, path, board,
         this](const std::string& name, uint64_t bus, uint8_t address) {
            return addI2CTarget(name, bus, address, systemConfiguration,
                                boardId, path, board);
        });

    iface->register_method(
        "RemoveMCTPI2CTarget", [&systemConfiguration, boardId, path, board,
                                this](const std::string& name) {
            removeI2CTarget(name, systemConfiguration, boardId, path, board);
        });
    dbus_interface::tryIfaceInitialize(iface);
}

} // namespace mctp_target
