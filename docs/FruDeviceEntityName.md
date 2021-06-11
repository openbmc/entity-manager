# Continuous integration and authorization for OpenBMC

Author:
  Willy Tu !wltu

Primary assignee:
  Willy Tu !wltu

Other contributors:

Created:
  2021-06-11

## Problem Description

In Entity Manager, there is no direct way of getting entity
names for the FRUs. The entity names can be useful for device
management to detemine which FRUs at which physcial location is missing or broken.

## Background and References

Google utlizes a static mapping with IPMI OEM handlers to provide the entity names to our system. It uses google-ipmi-sys's [getEntityName][2] with
[entity_association_map.json][1] to fetch the information.

## Requirements
- Fru Device will have access to the entity name/ physciont locaiton when the mapping is provided.
- Entity Manager will have the abaility to fetch `EntityName`
FruDevice when building the dbus objects.

## Proposed Design

The main idea is having a static entity association map that maps the entity id and instance to a entity name. Within
the  FruDevice, it will parse the json file to create the
mapping and add a new property under
`xyz.openbmc_project.FruDevice`.

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

This method is used google-ipmi-sys with IPMI OEM handler to
[getEntityName][2]. It does the following
1. Check if Entity Id is [supported][3].
2. Parse the entity association json.
3. Find the entity name that match the entity id and instance.

The following logic can be ported to Fru Device to do the
provide the Entity Manager the entity name information.

## Alternatives Considered

A design for [dynamic mux support][4] disscussed supprting
dynamic mux detection and configuration via Entity Manager. It
utilizes the exposed feature and will creating the I2C topology
off the mainboard. An extension that might be possible is to use
the the structure to provide a entity name in the entry. The
Entity Manager, can parse the structure to find the entry that
match the entity id and instance.

The issue with this is that the information is not provided to
FruDevice and is not the main intend for the design.

## Impacts

Extra work for FruDevice to parse the json for the mapping.

## Testing

Will test with Entity Manager + FruDevice.


[1]: https://github.com/openbmc/openbmc/blob/897b1454ff3be7d07d1a7057f4a23c0944aa7e87/meta-quanta/meta-gbs/recipes-gbs/gbs-ipmi-entity-association-map/gbs-ipmi-entity-association-map.bb
[2]: https://github.com/openbmc/google-ipmi-sys/blob/3b1b427c1fa4bcddcab1fc003410e5fa5d7a8334/handler.cpp#L235-L268
[3]: https://github.com/openbmc/google-ipmi-sys/blob/3b1b427c1fa4bcddcab1fc003410e5fa5d7a8334/handler_impl.hpp#L45-L58
[4]: https://gerrit.openbmc-project.xyz/c/openbmc/entity-manager/+/42971/
