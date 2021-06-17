#pragma once

#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using DbusVariant = std::variant<std::string, bool, uint8_t, uint16_t, int16_t,
                                 uint32_t, int32_t, uint64_t, int64_t, double>;

// Represent the return of object mapper GetSubStreePath call
using ObjectList = std::vector<std::string>;

// i2c root bus -> channel -> i2c child bus
using I2CChannelI2CMap =
    std::unordered_map<uint32_t, std::map<uint32_t, uint32_t>>;

// i2c child bus -> i2c root bus
using MuxI2CMap = std::unordered_map<uint32_t, uint32_t>;

// i2c bus -> slot object
using I2CSlotObjMap = std::unordered_map<uint32_t, std::string>;

/** @brief Build I2C topology by iterating "/sys/bus/i2c/devices" after
 * launching Entity Manager service
 */
void buildI2CTopology(I2CChannelI2CMap&, MuxI2CMap&);

/** @brief Get a DBus property by sending DBus call
 */
bool getDbusProperty(const char* service, const char* obj, const char* intf,
                     const char* property, DbusVariant&);
/** @brief Get a DBus property by sending DBus call
 */
bool setDbusPropery(const char* service, const char* obj, const char*,
                    const char* property, const DbusVariant&);

/** @brief Get DBus object paths by sending DBus call to Object Mapper service.
 * Assume that only Entity Manager has the input DBus interface.
 */
bool getObjectsFromEM(const std::vector<std::string>& infs, uint32_t depth,
                      ObjectList&);

/** @briet Convert the slot bus property that is formatted as
 * "$bus-Slot_$channel" to a bus number by looking up i2c channel i2c map
 */
std::optional<uint32_t> convertSlotBusProperty(const std::string&,
                                               const I2CChannelI2CMap&);

/**
 * Build I2C and slot DBus object path that managed by Entity Manager map.
 * Slot DBus objects have a I2C BUS property that is hardcoded in the entity
 * config or is a formatted string and can be determined by the I2C
 * topology
 */
void buildI2CSlotObjMap(const ObjectList&, const I2CChannelI2CMap&,
                        I2CSlotObjMap&);

/**
 * Find the connected slot I2C on the upstream entity when a downstream Entity
 * is added into the platform by inserting on a slot. This is done by looking up
 * I2C maps. Assume that the downstream entity EEPROM I2C bus or its ancestor
 * bus is on the upstream entity.
 */
std::optional<uint32_t> findConnectedSlotI2C(uint32_t i2c, const I2CSlotObjMap&,
                                             const MuxI2CMap&);
