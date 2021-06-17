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

void static buildI2CTopology(I2CChannelI2CMap& i2cChannelI2CMap,
                             MuxI2CMap& muxI2CMap)
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
            i2cChannelI2CMap[i2cBusRoot].emplace(channel, i2cBusChild);
            muxI2CMap[i2cBusChild] = i2cBusRoot;
        }
    }

    // Testing code
    std::cout << "I2C topology" << std::endl;
    for (const auto& item : i2cChannelI2CMap)
    {
        std::cout << item.first << " ";
        for (const auto& nestItem : item.second)
        {
            std::cout << nestItem.first << " " << nestItem.second << std::endl;
        }
    }
}

bool AssociationManager::getDbusProperty(const char* service, const char* obj,
                                         const char* intf, const char* property,
                                         DbusVariant& value)
{
    try
    {
        sdbusplus::message::message method =
            bus.new_method_call(service, obj, propertyIntf, "Get");
        method.append(intf, property);
        sdbusplus::message::message reply = bus.call(method);
        reply.read(value);
    }
    catch (sdbusplus::exception::SdBusError&)
    {
        return false;
    }

    return true;
}

bool AssociationManager::setDbusProperty(const char* service, const char* obj,
                                         const char* intf, const char* property,
                                         const DbusVariant& value)
{
    try
    {
        sdbusplus::message::message method =
            bus.new_method_call(service, obj, propertyIntf, "Set");
        method.append(intf, property);
        method.append(value);
        bus.call(method);
    }
    catch (sdbusplus::exception::SdBusError&)
    {
        return false;
    }

    return true;
}

bool AssociationManager::getObjectsFromEM(const std::vector<std::string>& intfs,
                                          uint32_t depth, ObjectList& objs)
{
    try
    {
        sdbusplus::message::message method = bus.new_method_call(
            objMapperService, objMapperObj, objMapperIntf, "GetSubTreePaths");
        method.append(entityInventoryPrefix, depth, intfs);
        sdbusplus::message::message reply = bus.call(method);
        reply.read(objs);
    }
    catch (sdbusplus::exception::SdBusError&)
    {
        return false;
    }

    return true;
}

bool AssociationManager::getEntityObjectsFromEM()
{
    return getObjectsFromEM(std::vector<std::string>({entityI2CIntf}), 2,
                            entityObjects);
}

bool AssociationManager::getSlotObjectsFromEM()
{
    return getObjectsFromEM(std::vector<std::string>({slotIntf}), 3,
                            slotObjects);
}

// In current upstream design, bus property will have the the form of
// "$bus.Slot_#_#", where $bus is the root bus on the board and # is the
// channel on the i2c mux. The logical bus number can be determined by looking
// up the system i2c topology.
std::optional<uint32_t>
    AssociationManager::convertSlotBusProperty(const std::string& property)
{
    auto pos = property.find(".");
    if (pos == std::string::npos)
    {
        return std::nullopt;
    }

    uint32_t i2c = std::stoi(property.substr(0, pos));
    size_t start;
    size_t end = pos + 6;
    if (property.size() < end)
    {
        return std::nullopt;
    };

    while ((start = property.find_first_not_of("_", end)) != std::string::npos)
    {
        end = property.find("_", start);
        uint32_t channel = std::stoi(property.substr(start, end - start));

        auto it = i2cChannelI2CMap.find(i2c);
        if (it == i2cChannelI2CMap.end())
        {
            return std::nullopt;
        }

        auto nestIt = it->second.find(channel);
        if (nestIt == it->second.end())
        {
            return std::nullopt;
        }

        i2c = nestIt->second;
    }

    return i2c;
}

void AssociationManager::buildI2CSlotObjMap()
{
    for (const auto& slot : slotObjects)
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
                auto busPtr = convertSlotBusProperty(val);
                if (!busPtr)
                {
                    i2cSlotObjMap.emplace(*busPtr, slot);
                }
            }
        }
    }
}

std::optional<uint32_t> AssociationManager::findConnectedSlotI2C(uint32_t i2c)
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

void AssociationManager::createAssociation(const std::string& forwardPath,
                                           const std::string& forwardType,
                                           const std::string& reversePath,
                                           const std::string& reverseType)
{
    auto object = associationIfaces.find(forwardPath);
    if (object == associationIfaces.end())
    {
        auto a =
            std::make_unique<AssociationObject>(bus, forwardPath.c_str(), true);

        using AssociationProperty =
            std::vector<std::tuple<std::string, std::string, std::string>>;
        AssociationProperty prop;

        prop.emplace_back(forwardType, reverseType, reversePath);
        a->associations(std::move(prop));
        associationIfaces.emplace(forwardPath, std::move(a));
    }
    else
    {
        // Interface exists, just update the property
        auto prop = object->second->associations();
        prop.emplace_back(forwardType, reverseType, reversePath);
        object->second->associations(std::move(prop));
    }
}

bool AssociationManager::createAssociation()
{
    if (!getSlotObjectsFromEM())
    {
        return false;
    }
    buildI2CSlotObjMap();

    if (!getEntityObjectsFromEM())
    {
        return false;
    }
    for (const auto& entity : entityObjects)
    {
        DbusVariant entityI2C;
        if (!getDbusProperty(emSerivice, entity.c_str(), entityI2CIntf, "Bus",
                             entityI2C))
        {
            continue;
        }
        auto i2c = std::get<uint32_t>(entityI2C);
        auto slotBusPtr = findConnectedSlotI2C(i2c);
        if (!slotBusPtr)
        {
            continue;
        }

        auto slotBus = *slotBusPtr;
        auto pos = i2cSlotObjMap[slotBus].find_last_of("/");
        if (pos != std::string::npos)
        {
            auto linkedEntity = i2cSlotObjMap[slotBus].substr(0, pos);
            createAssociation(entity, "containedby", linkedEntity, "contains");

            // Testing code
            std::cout << "Build association" << std::endl;
            std::cout << entity << std::endl;
            std::cout << linkedEntity << std::endl;

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

    return true;
}

int main()
{
    AssociationManager manager(std::move(sdbusplus::bus::new_system()));
    if (!manager.createAssociation())
    {
        return 1;
    }
    return 0;
}
