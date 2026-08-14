# Platform Configurations

## Overview

Platform configurations are a special category of entity-manager configurations
used to detect a **platform** (as opposed to a single entity) by detecting 2 or
more entities.

**Key Characteristics**:

1. Platform configurations detect 2 or more entities to identify a platform
2. They use `"Type": "Platform"` with an `Exposes` array; `"Type": "Platform"`
   routes validation to the platform schema (EMPlatformConfig)
3. They are validated at build time against the schema in `schemas/global.json`
   (EMPlatformConfig), which references `schemas/platform_schemas/`

They allow exposing configuration that can't be described against a single
entity. One example is configuration for MCTP over USB networks, which can
involve multiple USB hubs across entities.

## Location

Platform-specific configurations are stored in:

```text
configurations/platform/<vendor>/
```

## Schemas

Platform config schemas live under `schemas/platform/`:

## Associating a platform exposes record with a board

A platform `Exposes` record (for example an `MCTPUSBDevice`) names the board
inventory object its device is on in `Board`. Consumers use it to find the
configuration that board exposes for the device, and to associate the resulting
sensors with the correct board or chassis. Because the board must already exist
for this to resolve, it is expected as a `FOUND(...)` precondition in the
platform `Probe`.

## Describing the devices behind an MCTP bridge

An `MCTPUSBDevice` that acts as an MCTP bridge lists the devices reachable
through it in `BridgedEndpoints`. Each entry is a record in its own right and
may name a `Board` of its own, because a bridge and the devices behind it are
not necessarily on the same board; an entry that names none is on the bridge's.
The order matters: the Nth entry is assigned the Nth EID of the bridge's pool,
so entries must be listed in pool order.

## Pairing a device with the configuration its board exposes

A platform refers to a device by the name its board gives it. `Board`, described
above, tells a consumer which board to look in, and the platform record's `Name`
must match the `PlatformConfigName` of one of the records that board exposes.
The board defines the name and the platform looks it up, not the other way
round, so every platform that uses a board refers to its devices the same way.

For example, the IMGX ConnectX8 SuperNIC Switch board exposes one
`NvidiaMctpVdmSma` record for each of its eight SMAs, including:

```json
{
  "Name": "GPU_SMA_2 VDM",
  "PlatformConfigName": "GPU_SMA_2",
  "Type": "NvidiaMctpVdmSma"
}
```

A platform that reaches this SMA over USB points at that board with `Board` and
uses the board's name for the SMA as its own `Name`:

```json
{
  "Board": "Nvidia IMGX ConnectX8 SuperNIC Switch",
  "Name": "GPU_SMA_2",
  "Type": "MCTPUSBDevice"
}
```

All eight records share one type, so it is the name that picks this one. A
device behind an MCTP bridge is paired the same way: the `BridgedEndpoints`
entry's `Name` is matched against a `PlatformConfigName` on the entry's `Board`.

The matched name is also what the device is called on D-Bus. A board that is
instantiated more than once can template it, as long as each instance yields the
name its platform uses.
