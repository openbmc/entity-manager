# Associations

Entity Manager will create [associations][1] between entities in certain cases.

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
