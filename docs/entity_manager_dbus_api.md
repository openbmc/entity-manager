# Entity Manager DBus API

The Entity Manager is in charge of inventory management by scanning JSON files
and representing the data on DBus in a meaningful way. For more information on
the internal structure of the Entity Manager, please refer to the README of the
Entity Manager. This file documents a consumers usage of the entity manager, and
is not a description of the internal workings.

## DBus Object

### Object Paths

#### Entities: `/xyz/openbmc_project/Inventory/Item/{Entity Type}/{Entity Name}`

Entities are top level json objects that describe a piece of hardware. They are
groups of configurations with few properties of their own, they are a container
type for most pratical purposes.

#### Devices: `/xyz/openbmc_project/Inventory/Item/{Entity Type}/{Entity Name}/{Configuration}`

Configurations are components that are exposed when an entity is added to the
"global" system configuration. An example would be a TMP75 sensor that is
exposed when the front panel is detected.

**Example**:

```text
/xyz/openbmc_project/Inventory/Item/Board/Intel_Front_Panel/Front_Panel_Temp
```

### Interfaces

#### `xyz.openbmc_project.{InventoryType}`

See
[upstream types](https://github.com/openbmc/phosphor-dbus-interfaces/tree/master/yaml/xyz/openbmc_project/Inventory/Item)

- BMC
- Board
- Chassis
- CPU
- ...

These types closely align with Redfish types.

Entity objects describe pieces of physical hardware.

##### Properties

`unsigned int num_children`: Number of configurations under this entity.

`std::string name`: name of the inventory item

#### `xyz.openbmc_project.Configuration`

Configuration objects describe components owned by the Entity.

##### Properties

Properties will contain all non-objects (simple types) exposed by the JSON.

**Example**:

```text
path: /xyz/openbmc_project/Inventory/Item/Board/Intel_Front_Panel/Front_Panel_Temp
Interface: xyz.openbmc_project.Configuration
    string name = "front panel temp"
    string address = "0x4D"
    string "bus" = "1"
```

#### `xyz.openbmc_project.Device.{Object}.{index}`

{Object}s are members of the parent device that were originally described as
dictionaries. This allows for creating more complex types, while still being
able to get a picture of the entire system when doing a get managed objects
method call. Array objects will be indexed zero based.

##### Properties

All members of the dictonary.

**Example**:

```text
path: /xyz/openbmc_project/Inventory/Item/Board/Intel_Front_Panel/Front_Panel_Temp
Interface: xyz.openbmc_project.Device.threshold.0
    string direction = "greater than"
    int value = 55
```

## JSON Requirements

### JSON syntax requirements

Based on the above DBus object, there is an implicit requirement that device
objects may not have more than one level deep of dictionary or list of
dictionary. It is possible to extend in the future to allow nearly infinite
levels deep of dictonary with extending the
**xyz.openbmc_project.Device.{Object}** to allow
**xyz.openbmc_project.Device.{Object}.{SubObject}.{SubObject}** but that
complexity will be avoided until proven needed.

**Example**:

Legal:

```text
exposes :
[
    {
        "name" : "front panel temp",
        "property": {"key: "value"}
    }
]
```

Not Legal:

```text
exposes :
[
    {
        "name" : "front panel temp",
        "property": {
            "key": {
                "key2": "value"
            }
        }
    }
]
```

## Entity Manager PDI compatible DBus API

This is description about the new DBus API for configuration. For documentation
on the previous design, see above.

The existing API is still valid, this is simply describing the new API.

## Reasoning for the new API

There were some limitations in how entity-manager exposed configuration records
on DBus, which prevented using generated code from phosphor-dbus-interfaces to
access the configuration in a typesafe manner.

### Previous limitations

- Appending indices to dbus interface names created unbounded amount of dbus
  interfaces. e.g. `xyz.openbmc_project.Configuration.ADC.Thresholds${N}`
- EM simply dropped properties at a certain depth.
- Nesting of dbus interface names prevented reuse without duplication. e.g.
  `xyz.openbmc_project.Configuration.${Type}.FirmwareInfo` in which case the
  interface had the same properties with the same types, still appeared as
  something different.

### Benefits of using generated client

- no need for hard-coding of DBus property names
- no need for hard-coding of DBus interface names
- type-safe access for properties

### Object Paths

The object paths represent the structure of the json object in the
configuration. There are no depth-based edge cases beyond the toplevel.

- If a property has a primitive data type, it becomes part of the DBus
  interface.
- If a property is an array of a primitve data type, it becomes part of the DBus
  interface.
- If a property has an object as its value, the property name is appended to the
  object path.
- If a property has an array of objects as its value, the property name is
  appended to the object path. The index of those objects in the array then also
  becomes part of the object path.

#### Entities: `/xyz/openbmc_project/Inventory/Item/{Entity Type}/{Entity Name}`

## Example Record loosely based on schemas/firmware.json

```json
{
    "Name": "MyRecord",
    "SPIControllerIndex": 1,
    "SPIDeviceIndex": 0,
    "FirmwareInfo": {
        "VendorIANA": 3733,
        "CompatibleHardware": "com.abcd.myboard.Host.SPI",
    },
    "MuxOutputs": [
        {
            "Name": "SPI_SEL_0",
            "Polarity": "High",
        },
        {
            "Name": "SPI_SEL_1",
            "Polarity": "Low",
        }
    ]
    "Type": "SPIFlash",
}
```

## Resulting tree

```text
|- /xyz/openbmc_project/inventory/system/board/chassis/MyRecord
| |- /xyz/openbmc_project/inventory/system/board/chassis/MyRecord/FirmwareInfo
| `- /xyz/openbmc_project/inventory/system/board/chassis/MyRecord/MuxOutputs
|   |- /xyz/openbmc_project/inventory/system/board/chassis/MyRecord/MuxOutputs/0
|   `- /xyz/openbmc_project/inventory/system/board/chassis/MyRecord/MuxOutputs/1
```

## Access the example record

### Access the property "Polarity" (object path and DBus interface)

```text
/xyz/openbmc_project/inventory/system/board/myboard/MyRecord/MuxOutputs/1
xyz.openbmc_project.Configuration.MuxOutput
```

## JSON Requirements for exposes records

The toplevel configuration record has to have the `Type` property.

The toplevel configuration record needs to have the `Name` property.
