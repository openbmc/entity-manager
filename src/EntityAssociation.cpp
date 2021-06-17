#include "EntityAssociation.hpp"

#include <systemd/sd-journal.h>

#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/message.hpp>

#include <filesystem>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <variant>
#include <vector>

static constexpr auto entityInventoryPrefix =
    "/xyz/openbmc_project/inventory/system";
static constexpr auto emSerivice = "xyz.openbmc_project.EntityManager";
static constexpr auto slotIntf = "xyz.openbmc_project.Configuration.EntitySlot";
static constexpr auto entityI2CIntf =
    "xyz.openbmc_project.Inventory.Decorator.I2Cdevice";
static constexpr auto entityLocationIntf =
    "xyz.openbmc_project.Inventory.Decorator.LocationCode";
static constexpr auto objMapperService = "xyz.openbmc_project.ObjectMapper";
static constexpr auto objMapperIntf = "xyz.openbmc_project.ObjectMapper";
static constexpr auto objMapperObj = "/xyz/openbmc_project/object_mapper";
static constexpr auto propertyIntf = "org.freedesktop.DBus.Properties";

static auto bus =
    std::make_unique<sdbusplus::bus::bus>(sdbusplus::bus::new_default_system());

void buildI2CTopology(I2CChannelI2CMap& i2cChannelI2CMap, MuxI2CMap& muxI2CMap)
{
    for (const auto& p :
         std::filesystem::directory_iterator("/sys/bus/i2c/devices"))
    {
        if (!std::filesystem::is_directory(p))
        {
            continue;
        }

        std::string path = p.path();
        std::smatch i2cBusAddr;
        if (std::regex_match(path, i2cBusAddr,
                             std::regex(".+(\\d+)-([0-9abcdef]+$)")) ||
            i2cBusAddr.size() != 3)
        {
            continue;
        }

        uint32_t i2cBusRoot = std::stoi(i2cBusAddr[1]);
        for (const auto& sp : std::filesystem::directory_iterator(p))
        {
            std::string spath = sp.path();
            std::smatch muxChannel;
            if (std::regex_match(spath, muxChannel,
                                 std::regex(".+(channel-)(\\d+$)")) ||
                muxChannel.size() != 3)
            {
                continue;
            }

            uint32_t channel = std::stoi(muxChannel[2]);
            auto ec = std::error_code();
            auto symlinkPath = std::filesystem::read_symlink(sp, ec);
            if (ec)
            {
                continue;
            }

            std::string symlinkPathStr = symlinkPath.string();
            auto findBus = symlinkPathStr.find_last_of("-");
            if (findBus == std::string::npos ||
                findBus + 1 == symlinkPathStr.size())
            {
                continue;
            }

            uint32_t i2cBusChild =
                std::stoi(symlinkPathStr.substr(findBus + 1));
            i2cChannelI2CMap[i2cBusRoot][channel] = i2cBusChild;
            muxI2CMap[i2cBusChild] = i2cBusRoot;
        }
    }
}

bool getDbusProperty(const char* service, const char* obj, const char* intf,
                     const char* property, DbusVariant& value)
{
    try
    {
        sdbusplus::message::message method =
            bus->new_method_call(service, obj, propertyIntf, "Get");
        method.append(intf, property);
        sdbusplus::message::message reply = bus->call(method);
        reply.read(value);
    }
    catch (sdbusplus::exception::SdBusError&)
    {
        return false;
    }

    return true;
}

bool setDbusProperty(const char* service, const char* obj, const char* intf,
                     const char* property, DbusVariant& value)
{
    try
    {
        sdbusplus::message::message method =
            bus->new_method_call(service, obj, propertyIntf, "Set");
        method.append(intf, property);
        method.append(value);
        bus->call(method);
    }
    catch (sdbusplus::exception::SdBusError&)
    {
        return false;
    }

    return true;
}

bool getObjectsFromEM(const std::vector<std::string>& intfs, uint32_t depth,
                      ObjectList& objs)
{
    try
    {
        sdbusplus::message::message method = bus->new_method_call(
            objMapperService, objMapperObj, objMapperIntf, "GetSubTreePaths");
        method.append(entityInventoryPrefix, depth, intfs);
        sdbusplus::message::message reply = bus->call(method);
        reply.read(objs);
    }
    catch (sdbusplus::exception::SdBusError&)
    {
        return false;
    }

    return true;
}

std::optional<uint32_t>
    convertSlotBusProperty(const std::string& property,
                           const I2CChannelI2CMap& i2cChannelI2CMap)
{
    auto pos = property.find(".");
    if (pos == std::string::npos)
    {
        return std::nullopt;
    }

    uint32_t i2c = std::stoi(property.substr(0, pos));
    std::string trimStr = property.substr(pos + 1);
    std::vector<std::string> splitStrs;
    size_t start;
    size_t end = 0;

    // Split the string by "_"
    while ((start = trimStr.find_first_not_of("_", end)) != std::string::npos)
    {
        end = trimStr.find("_", start);
        splitStrs.push_back(trimStr.substr(start, end - start));
    }

    if (splitStrs.size() < 2 || splitStrs[0] != "Slot")
    {
        return std::nullopt;
    }

    for (size_t i = 1; i < splitStrs.size(); i++)
    {
        auto it = i2cChannelI2CMap.find(i2c);
        if (it == i2cChannelI2CMap.end())
        {
            return std::nullopt;
        }

        uint32_t channel = std::stoi(splitStrs[i]);
        auto nestIt = it->second.find(channel);
        if (nestIt == it->second.end())
        {
            return std::nullopt;
        }

        i2c = nestIt->second;
    }

    return i2c;
}

void buildI2CSlotObjMap(const ObjectList& slots,
                        const I2CChannelI2CMap& i2cChannelI2CMap,
                        I2CSlotObjMap& i2cSlotObjMap)
{
    for (const auto& slot : slots)
    {
        DbusVariant i2c;
        if (getDbusProperty(emSerivice, slot.c_str(), slotIntf, "Bus", i2c))
        {
            try
            {
                auto val = std::get<uint32_t>(i2c);
                i2cSlotObjMap.emplace(val, slot);
            }
            catch (const std::bad_variant_access&)
            {
                auto val = std::get<std::string>(i2c);
                auto busPtr = convertSlotBusProperty(val, i2cChannelI2CMap);
                if (!busPtr)
                {
                    i2cSlotObjMap.emplace(*busPtr, slot);
                }
            }
        }
    }
}

std::optional<uint32_t> findConnectedSlotI2C(uint32_t i2c,
                                             const I2CSlotObjMap& i2cSlotObjMap,
                                             const MuxI2CMap& muxI2CMap)
{
    while (i2cSlotObjMap.find(i2c) == i2cSlotObjMap.end() &&
           muxI2CMap.find(i2c) != muxI2CMap.end())
    {
        auto it = muxI2CMap.find(i2c);
        if (it != muxI2CMap.end())
        {
            i2c = it->second;
        }
    }

    if (i2cSlotObjMap.find(i2c) != i2cSlotObjMap.end())
    {
        return i2c;
    }
    return std::nullopt;
}

int main()
{
    I2CChannelI2CMap i2cChannelI2CMap;
    MuxI2CMap muxI2CMap;
    buildI2CTopology(i2cChannelI2CMap, muxI2CMap);

    ObjectList slots;
    I2CSlotObjMap i2cSlotObjMap;
    if (!getObjectsFromEM(std::vector<std::string>({slotIntf}), 3, slots))
    {
        return 1;
    }

    buildI2CSlotObjMap(slots, i2cChannelI2CMap, i2cSlotObjMap);
    ObjectList entities;
    if (!getObjectsFromEM(std::vector<std::string>({entityI2CIntf}), 2,
                          entities))
    {
        return 1;
    }

    for (const auto& entity : entities)
    {
        DbusVariant entityI2C;
        if (!getDbusProperty(emSerivice, entity.c_str(), entityI2CIntf, "Bus",
                             entityI2C))
        {
            continue;
        }
        auto i2c = std::get<uint32_t>(entityI2C);
        auto slotBusPtr = findConnectedSlotI2C(i2c, i2cSlotObjMap, muxI2CMap);
        if (!slotBusPtr)
        {
            continue;
        }

        auto slotBus = *slotBusPtr;
        auto pos = i2cSlotObjMap[slotBus].find_last_of("/");
        if (pos != std::string::npos)
        {
            auto linkedEntityObj = i2cSlotObjMap[slotBus].substr(0, pos);

            // todo: create association between connected entities
            // createAssociation(entity, linkedEntity, "containedby",
            // "contain");

            DbusVariant slotLocation;
            if (getDbusProperty(emSerivice, i2cSlotObjMap[slotBus].c_str(),
                                slotIntf, "Location", slotLocation))
            {
                if (!setDbusProperty(emSerivice, entity.c_str(),
                                     entityLocationIntf, "LocationCode",
                                     slotLocation))
                {
                    std::cerr << "Fail to set up entity LocationCode\n";
                }
            }
        }
    }
}
