# Associations

Entity Manager will create [associations][1] between entities in certain cases.

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
statement matches properties to a path, this path is set as the probed path
in the inventory item.

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

This configuration queries the object mapper to find an FruDevice interface
with the stated property values (BOARD_PRODUCT_NAME, PRODUCT_NAME).
The path `/xyz/openbmc_project/FruDevice/Management_Board_wBMC` is found, which
means EM loads the configuration. The found path is the probed path and is
placed as the probing/probed_by association property in the inventory item of
the configuration.

## Deprecated configuration style

The configuration style described below is deprecated and superseded.

## `contained_by`, `containing`

Entity Manager can model the [physical topology][2] of how entities plug into
each other when upstream and downstream ports are added as `Exposes` elements.
It will then create the 'upstream containing downstream' and 'downstream
contained_by upstream' associations for the connected entities.

For example, taken from the referenced physical topology design:

superchassis.json:

```json
{
  "Exposes": [
    {
      "Name": "MyPort",
      "Type": "BackplanePort"
    }
  ],
  "Name": "Superchassis"
}
```

subchassis.json:

```json
{
  "Exposes": [
    {
      "ConnectsToType": "BackplanePort",
      "Name": "MyDownstreamPort",
      "Type": "DownstreamPort"
    }
  ],
  "Name": "Subchassis"
}
```

Entity Manager will create the 'Superchassis containing Subchassis' and
'Subchassis contained_by Superchassis` associations, putting the associations
definition interface on the downstream entity.

## `powered_by`, `powering`

In addition to the `containing` associations, entity-manager will add
`powering`/`powered_by` associations between a power supply and its parent when
its downstream port is marked as a `PowerPort`.

The below example shows two PSU ports on the motherboard, where the `Type`
fields for those ports match up with the `ConnectsToType` field from the PSUs.

motherboard.json:

```json
{
  "Exposes": [
    {
      "Name": "PSU 1 Port",
      "Type": "PSU 1 Port"
    },
    {
      "Name": "PSU 2 Port",
      "Type": "PSU 2 Port"
    }
  ]
}
```

psu.json:

```json
{
  "Exposes": [
    {
      "ConnectsToType": "PSU$ADDRESS % 4 + 1 Port",
      "Name": "PSU Port",
      "Type": "DownstreamPort",
      "PowerPort": true
    }
  ]
}
```

[1]:
  https://github.com/openbmc/docs/blob/master/architecture/object-mapper.md#associations
[2]: https://github.com/openbmc/docs/blob/master/designs/physical-topology.md
