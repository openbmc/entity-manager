# gpio-presence-sensor

This program was originally implemented following the design [1].

## Configuration

See [1] for the full design.

Example EM config fragments:

```
{
  Exposes:
  [
    {
      "Name": "com.meta.Hardware.Yv4.cable0",
      "PresencePinNames": ["presence-cable0"],
      "PresencePinValues": [1],
      "Type": "GPIODeviceDetect"
    },
    {
      "Name": "com.meta.Hardware.Yv4.ComputeCard",
      "PresencePinNames": ["presence-slot0a", "presence-slot0b"],
      "PresencePinValues": [0, 1],
      "Type": "GPIODeviceDetect"
    },
    {
      "Name": "com.meta.Hardware.Yv4.SidecarExpansion",
      "PresencePinNames": ["presence-slot0a", "presence-slot0b"],
      "PresencePinValues": [1, 0],
      "Type": "GPIODeviceDetect"
    },
    {
      "Name": "com.meta.Hardware.Yv4.AirBlocker",
      "PresencePinNames": ["presence-slot0a", "presence-slot0b"],
      "PresencePinValues": [1, 1],
      "Type": "GPIODeviceDetect"
    },
    {
      "Name": "com.meta.Hardware.Yv4.fanboard0",
      "PresencePinNames": ["presence-fanboard0"],
      "PresencePinValues": [0],
      "Type": "GPIODeviceDetect"
    },
    ...
  ],
  ...
  "Name": "My Chassis",
  "Probe": "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'MYBOARDPRODUCT*'})",
  "Type": "Board",
}
```

The above configuration can then cause a Probe match in another configuration,
like below:

```
{
  Exposes:
  [
      {
        "Address": "0x28",
        "Bus": 5,
        "EntityId": 7,
        "EntityInstance": 0,
        "Name": "fanboard_air_inlet",
        "Name1": "fanboard_air_outlet",
        "Type": "NCT7802"
    },
    ...
  ],
  ...
  "Name": "My Fan Board 0",
  "Probe": "xyz.openbmc_project.Inventory.Source.DevicePresence({'Name': 'com.meta.Hardware.Yv4.fanboard0'})",
  "Type": "Board",
}
```

Notice the **xyz.openbmc_project.Inventory.Source.DevicePresence** interface.
This is what the gpio-presence daemon exposes on dbus when the hardware is
detected as present. The **Name** property in the Probe statement is the same as
configured as in the first json fragment.

## Applications

Applications include detecting fan boards, air blockers, cables and other simple
components for which no standard / well-defined way exists to detect them
otherwise.

It can also be used as detection redundancy in case another detection mechanism
like FRU eeprom is corrupted or unavailable.

## References

- [1] https://gerrit.openbmc.org/c/openbmc/docs/+/74022
- [1]
  https://github.com/openbmc/docs/blob/master/designs/inventory/gpio-based-hardware-inventory.md
