# Associations

Entity Manager will create [associations][1] between entities in certain cases.
The associations are needed as part of [2].

## Configuring Associations between Entities

The configuration record has `Name` field which is used to connect 2 ports for
an association definition.

If a matching element with `Name` is not found, that is not an error, it simply
means the component we want to associate to is not present.

The `PortType` describes which association to create. This is limited to
pre-defined values. It also defines the direction of the association.

### containing Association

Baseboard configuration.

```json
{
  "Exposes": [
    {
      "Name": "ContainingPort",
      "PortType": "contained_by"
      "Type": "Port"
    }
  ],
  "Name": "Tyan S8030 Baseboard"
}
```

Chassis configuration.

```json
{
  "Exposes": [
    {
      "Name": "ContainingPort",
      "PortType": "containing"
      "Type": "Port"
    }
  ],
  "Name": "MBX Chassis"
}
```

### powering Association

Baseboard configuration. This baseboard accepts one of several generic PSUs.

```json
{
  "Exposes": [
    {
      "Name": "GenericPowerPort",
      "PortType": "powered_by"
      "Type": "Port"
    }
  ],
  "Name": "Tyan S8030 Baseboard"
}
```

PSU configuration. This example PSU is generic and can be used on different
servers.

```json
{
  "Exposes": [
    {
      "Name": "GenericPowerPort",
      "PortType": "powering"
      "Type": "Port"
    }
  ],
  "Name": "Generic Supermicro PSU"
}
```

### probing Association

The probing association matches an entry in the inventory to a probed path and
depends on the 'Probe' statement of an EM configuration. If the 'Probe'
statement matches properties to a path, this path is set as the probed path in
the inventory item.

For example 'yosemite4.json':

```json
{
    "Exposes": [
        ...
    ],
    "Name": "Yosemite 4 Management Board",
    "Probe": "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'Management Board wBMC', 'PRODUCT_PRODUCT_NAME': 'Yosemite V4'})",
    "Type": "Board",
    ...
}
```

This configuration queries the object mapper to find an FruDevice interface with
with the stated property values (BOARD_PRODUCT_NAME, PRODUCT_NAME). The path
`/xyz/openbmc_project/FruDevice/Management_Board_wBMC` is found, which means EM
loads the configuration. The found path is the probed path and is placed as the
probing/probed_by association property in the inventory item of the
configuration.

[1]:
  https://github.com/openbmc/docs/blob/master/architecture/object-mapper.md#associations
[2]: https://github.com/openbmc/docs/blob/master/designs/physical-topology.md
