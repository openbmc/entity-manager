# NVMe Device Configuration

The optional `nvme-device` daemon uses a separate platform-specific JSON file,
not an Entity Manager probe configuration. By default, it reads
`/usr/share/entity-manager/nvme_device.json`. An alternate file path can be
passed as the daemon's first argument.

```json
{
  "config": [
    { "Index": 4, "Name": "E1S_BOOT", "BusID": 24 },
    { "Index": 0, "BusID": 25 }
  ]
}
```

- `Index`: required unsigned 32-bit device index.
- `BusID`: required unsigned 16-bit I2C bus number. It must match the platform's
  bus mapping.
- `Name`: optional D-Bus object path element. It must contain only letters,
  digits and underscores. If omitted, the name defaults to `nvme<Index>`.

Each entry must have a unique `Index`, `BusID`, and resulting name. The
platform supplies the file; these bus numbers are examples and are not defaults.
