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

## Pairing a platform record with its board's configuration

A platform record finds the configuration its board exposes for the device by
`Board` and `Name`: `Board` names the board inventory object to look in, and
`Name` must match the `Name` of one of the records that board exposes. A
record's `Name` only has to be unique within its board, so it is `Board` that
says which board's records are meant.

For example, the IMGX ConnectX8 SuperNIC Switch board exposes one
`NvidiaMctpVdmSma` record for each of its SMAs, including:

```json
{
  "Name": "GPU_SMA_2",
  "Type": "NvidiaMctpVdmSma"
}
```

A platform that reaches this SMA over USB refers to it as:

```json
{
  "Board": "Nvidia IMGX ConnectX8 SuperNIC Switch",
  "Name": "GPU_SMA_2",
  "Type": "MCTPUSBDevice"
}
```

A `BridgedEndpoints` entry is paired the same way, against its own `Board`, or
the bridge's when it names none.
