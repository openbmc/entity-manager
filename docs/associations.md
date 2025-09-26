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

- Downstream Type:Port stanza is marked as a PortType `powered_by` and
- Upstream Type:Port stanza(s) are marked with PortType `powering`
- Ports are linked via matching 'Name' fields in the Type:Port stanzas.

The below example shows two PSU ports on the motherboard, where the `PortType`
fields for those ports match up with the `ConnectsToType` field from the PSUs.

motherboard.json:

```json
{
  "Exposes": [
    {
      "Name": "PSU 1 Port",
      "Type": "Port",
      "PortType": "powered_by"
    },
    {
      "Name": "PSU 2 Port",
      "Type": "Port",
      "PortType": "powered_by"
    }
  ]
}
```

psu.json:

```json
{
  {
    "Exposes": [
    {
      "Name": "PSU$ADDRESS % 4 + 1 Port",
      "Type": "Port",
      "PortType": "powering"
    }
    "Type": "PowerSupply",
    "Probe": ...
    ]
  }
}
```

## `cooled_by`, `cooling`

In addition to the `containing` associations, entity-manager will add
`cooling`/`cooled_by` associations between a fan and its parent when:

- Downstream Type:Port stanza is marked as a PortType `cooled_by` and
- Upstream Type:Port stanza(s) are marked with PortType `cooling`
- Ports are linked via matching 'Name' fields in the Type:Port stanzas.

The below example shows three Fan ports on the motherboard, where the `Name`
`PortType` field from the Fans.

Depending on your fan configuration, it may make sense to set probes for each
fan slot entity to trigger on specific chassis configs being active, or based on
specific asset decorations being set.

motherboard.json:

```json
{
  "Exposes": [
    {
      "Name": "Fan Single Port 1",
      "Type": "Port",
      "PortType": "cooled_by"
    },
    {
      "Name": "Fan Many Port",
      "Type": "Port",
      "PortType": "cooled_by"
    }
  ]
}
```

fan.json:

```json
{
  {
    "Exposes": [
      {
        "Name": "Fan Single Port 1",
        "Type": "Port",
        "PortType": "cooling"
      },
      {
       //fan 1 stanza
      },
      "Type": "Fan",
      "Probe": ...
    ]
  },
  {
    "Exposes": [
      {
        "Name": "Fan Many Port",
        "Type": "Port",
        "PortType": "cooling"
      },
      {
        //fan 2 stanza
      },
    "Type": "Fan",
      "Probe": ...
    ]
  },
  {
    "Exposes": [
      {
        "Name": "Fan Many Port",
        "Type": "Port",
        "PortType": "cooling"
      },
      {
        //fan 3 stanza
      },
    "Type": "Fan",
    "Probe": ...
    ]
  }
}
```

[1]:
  https://github.com/openbmc/docs/blob/master/architecture/object-mapper.md#associations
[2]: https://github.com/openbmc/docs/blob/master/designs/physical-topology.md
