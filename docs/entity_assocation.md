# Platform physical model : association between entities

Author: Jie Yang (jjy)

Created: June 10, 2021

## Problem Description

Current [Entity Manager](https://github.com/openbmc/entity-manager) does not
support to detect the physical connectivity between entities. But platform
management software is interested how an entity is added into the platform, when
the entity is detected. For example, an expansion card is inserted on the PCIE
slot 0 of the baseboard. This connectivity information is required when building
the platform physical model.

## Background

Many platforms can have multiple chassis, trays, risers, drives, et al. that own
entity config in Entity Manager. Platform management software shall recognize
how those entities are connected to the baseboard that is typically where CPUs
locate via Redfish. With such connectivity info platform management software can
report the location of any inactive entities.

## Requirements
1. The connectivity info should be exposed to DBus interface. If there is a
cable that connects two entities, cable info should also be present.
2. The connectivity info is dynamical, when an entity can be plugged into
different slots (e.g.  a PCIE device is inserted on PCIE slot 0 or PCIE slot 1).
Entity manager should detect the connectivity change.

## Solution

### Expose connector as device

All connectors like PCIE slots, riser connectors and NIC connectors will be
exposed as devices in the entity config with an abstract type `EntityConnector`
and a hardcoded name that is typically the silkscreen name of the connector. The
connector will also have an i2c bus number property. The bus number may be
identical with entity EEPROM bus; or a different physical i2c bus on the
baseboard that will need to be hardcoded in entity config; or a logical bus that
is behind a mux device and will be dynamically determined  when loading the mux
driver.

Entity manger will publish each connector with a DBus object
`/xyz/openbmc_project/Inventory/Item/{Entity Type}/{Entity Name}/{ConnectorName}
`. The connector object path will have a DBus interface
`xyz.openbmc_project.Configuration.EntityConnector` with properties
`Bus`, `Name` and `Type`. Using object mapper, we can build a map between i2c
bus and the connector object path, which is supposed to be a 1:1 mapping.

### Associate entity with connector

When an entity EEPROM is detected, Entity Manager can atttach a DBus interface
`xyz.openbmc_project.Inventory.Decorator.I2CDevice` with i2c bus and address
properties to the entity object path `/xyz/openbmc_project/Inventory/Item/
{Entity Type}/{Entity Name}`. Then we can start build association between
entity and entity connector. The name of forward and reverse object created by
the association are defined as `assembly` and `link`. There would be two cases
to discuss:

 * The entity i2c bus is mapped to a connector. Then directly associate the
 entity object with the connector object.
 * The entity i2c bus is not in the i2c and connector map. In this case, the
 entity i2c bus would be the i2c bus of baseboard EEPROM or a logical bus that
 is behind a mux on board. For example, an entity EEPROM has i2c bus 70 behind
 a mux and the entity is inserted on the i2c bus 53 that is mapped a connector
 -- PCIE slot 0 on the baseboard. The i2c bus 70 will not be in the map. Thus we
will need to trace the i2c bus in front of the mux recursively by iterating the
system i2c bus directory until a connector is found and then build the
association. If we cannot find a connector, which means the entity is the root
entity - baseboard. No need to build assocation to the baseboard.
