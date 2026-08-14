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

A platform `Exposes` record (for example an `MCTPUSBDevice`) may carry a `Board`
property naming the board inventory object the record's device is on. Consumers
use it to find the configuration that board exposes for the device, and to
associate the resulting sensors with the correct board or chassis. Because the
board must already exist for this to resolve, it is expected as a `FOUND(...)`
precondition in the platform `Probe`.

## Describing the devices behind an MCTP bridge

An `MCTPUSBDevice` that acts as an MCTP bridge lists the devices reachable
through it in `BridgedEndpoints`. Each entry is a record in its own right, with
its own `Board`, because a bridge and the devices behind it are not necessarily
on the same board. The order matters: the Nth entry is assigned the Nth EID of
the bridge's pool, so entries must be listed in pool order.
