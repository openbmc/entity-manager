# Continuous integration and authorization for OpenBMC

Author:
  Willy Tu !wltu

Primary assignee:
  Willy Tu !wltu

Other contributors:

Created:
  2021-06-11

## Problem Description

In Entity Manager, there is no direct way of getting entity names for the FRUs.
The entity name can be anything we want, but can be useful to provide the
physical location information.

The physical locations can be useful for device management to detemine the FRUS
that is missing or broken.

## Background and References

Google utlizes a static mapping with IPMI OEM commands to provide the entity
names for our system. It uses google-ipmi-sys's [getEntityName][2] along with
[entity_association_map.json][1] to fetch the information.

With the example of physical location, it is impossible to detect dynamically
since it's based on details of how the board is laid out and how the silkscreen
is labeled. Having a static mapping from entity id/instance to a name will
provide the most flexible and accurate information.

## Requirements
- Static mapping of entity id/instance to entity name
  - entity instance is configured by the user, so all valid instance should map
  to a name when possible.
- Entity Manager will provide `EntityName` property for the FRUs under
`xyz.openbmc_project.Inventory.Decorator.Ipmi`.

## Proposed Design

The main idea is having a static entity association map that maps the entity id
and instance to a entity name. Within the EntityManager, it will parse the json
file to create the mapping and add a `EntityName` property under
`xyz.openbmc_project.Inventory.Decorator.Ipmi`.

Example [entity_association_map.json][1]
```json
{
    "system_board": [
        {"instance": 1, "name": "/"},
        {"instance": 2, "name": "/i2cool_0"},
        {"instance": 3, "name": "/i2cool_1"},
        {"instance": 4, "name": "/i2cool_2"}
    ],
    "system_internal_expansion_board": [
        {"instance": 1, "name": "/"}
    ],
    "power_system_board": [
        {"instance": 1, "name": "/"}
    ],
    "add_in_card": [
        {"instance": 0, "name": "/PE0"},
        {"instance": 1, "name": "/PE1"}
    ],
    "fan": [
        {"instance": 0, "name": "/fan0"},
        {"instance": 1, "name": "/fan1"},
        {"instance": 2, "name": "/fb_fan0"},
        {"instance": 3, "name": "/fb_fan1"},
        {"instance": 4, "name": "/fb_fan2"}
    ],
    "cooling_unit": [
        {"instance": 0, "name": "/ZONE0"},
        {"instance": 1, "name": "/ZONE1"},
        {"instance": 2, "name": "/ZONE2"}
    ]
}
```

### Implementation Details

The implementation will be based on [getEntityName][2] used in google-ipmi-sys
with IPMI OEM handler. It does the following
1. Check if Entity Id is [supported][3].
2. Parse the entity association json to create the mapping.
3. Find the entity name that match the entity id/instance.

For the EntityManager Implementation, it will include the following steps.
1. Parse the entity association json to create the mapping. This is cached and
only needs to be done once.
2. Use the mapping to search the entity name with entity id/instance.

Due to the nature of EntityManager parsing all of the configuration jsons to
create the dbus object for the FRUs, the `getEntityManager` method will be
called multiple times during the initial setup and some more as more devices
gets added after boot time. The `entity_association_map.json` will be parsed and
cached as a hashmap for future uses.

```
entityNameMap = {
  {1, {1, "PE0}},
  {1, {2, "PE1}},
  ...
}
```

With the mapping ready, `entityNameMap[id][instance]` will be the entity name.

## Alternatives Considered

A design for [dynamic mux support][4] disscussed supprting
dynamic mux detection and configuration via Entity Manager. It
utilizes the exposed feature and will creating the I2C topology
off the mainboard. An extension that might be possible is to use
the the structure to provide a entity name in the entry. The
Entity Manager, can parse the structure to find the entry that
match the entity id and instance.

The issue with this is that the information is not the main intend for the
design and required some weird mapping between exposed path the FRUs.

## Impacts

Extra feature for EntityManager to parse the json for the mapping.

## Testing

Will test with Entity Manager + static mapping.


[1]: https://github.com/openbmc/openbmc/blob/897b1454ff3be7d07d1a7057f4a23c0944aa7e87/meta-quanta/meta-gbs/recipes-gbs/gbs-ipmi-entity-association-map/gbs-ipmi-entity-association-map.bb
[2]: https://github.com/openbmc/google-ipmi-sys/blob/3b1b427c1fa4bcddcab1fc003410e5fa5d7a8334/handler.cpp#L235-L268
[3]: https://github.com/openbmc/google-ipmi-sys/blob/3b1b427c1fa4bcddcab1fc003410e5fa5d7a8334/handler_impl.hpp#L45-L58
[4]: https://gerrit.openbmc-project.xyz/c/openbmc/entity-manager/+/42971/
