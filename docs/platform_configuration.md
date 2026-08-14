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

`Board` says which board to look in, and the record's name says which of that
board's records to use. A board describes each of its devices with a record
carrying an `InventoryName`, and a platform names the same device with that
string: `Name` for the device itself, and the entry's `Name` for a bridged one.
A board can carry several records a consumer cannot otherwise tell apart, so the
name is what identifies one of them.

That name is also what the device is called on D-Bus. Keeping it in the board's
configuration rather than the platform's means a platform does not have to be
edited when a device is renamed, and a board that is instantiated more than once
can template it.
