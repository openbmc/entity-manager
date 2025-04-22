# Entity Manager alternative DBus API

This is description about the alternative DBus API for configuration. For
documentation on the previous design, refer to
**[Entity Manager DBus API](https://github.com/openbmc/entity-manager/blob/master/docs/entity_manager_dbus_api.md)**
The existing API is still valid, this is simply describing the alternative API.

## Reasoning for the alternative API

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

## Example Record

```json
{
    "Name": "MyRecord",
    "Property": {
        "T": 3,
        "L": [1,2,3,4],
        "Type": "Y"
    },
    "Property2": {
        "D": {
            "A": 7,
            "Type": "K"
        },
        "Type": "J"
    },
    "ArrayProperty": [
        {
            "C": 8,
            "Type": "Z"
        },
        {
            "C": 6,
            "Type": "Z"
        }
    ]
    "Type": "X",
}
```

## Resulting tree

```
|- /xyz/openbmc_project/inventory/system/board/chassis/MyRecord
| |- /xyz/openbmc_project/inventory/system/board/chassis/MyRecord/Property
| |- /xyz/openbmc_project/inventory/system/board/chassis/MyRecord/Property2
| | `- /xyz/openbmc_project/inventory/system/board/chassis/MyRecord/Property2/D
| `- /xyz/openbmc_project/inventory/system/board/chassis/MyRecord/ArrayProperty
|   |- /xyz/openbmc_project/inventory/system/board/chassis/MyRecord/ArrayProperty/0
|   `- /xyz/openbmc_project/inventory/system/board/chassis/MyRecord/ArrayProperty/1
```

## Access the example record

### Access the property "T" (object path and DBus interface)

```
/xyz/openbmc_project/inventory/system/board/myboard/MyRecord/Property
xyz.openbmc_project.Configuration.Y
```

### Access the property "C" (object path and DBus interface)

```
/xyz/openbmc_project/inventory/system/board/myboard/MyRecord/ArrayProperty/1
xyz.openbmc_project.Configuration.Z
```

## JSON Requirements

All json objects part of configuration records at all levels need to have the
`Type` property to form the DBus interface name.

The toplevel configuration record needs to have the `Name` property.
