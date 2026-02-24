# Blacklist Configuration

The blacklist.json in package directory can determine i2c buses and addresses
that should not be scanned by FruDevice. An integer blocks an entire bus from
being scanned. A bus/addresses object can block specific addresses on the bus
while allowing scanning others addresses on the same bus.

## For buses

Put in numbers of buses. For example:

```json
{
  "buses": [1, 3, 5]
}
```

Note that "buses" should be an array of unsigned integer.

## For addresses

Put in bus and addresses with this format:

```json
{
  "buses": [
    {
      "bus": 3,
      "addresses": ["0x30", "0x40"]
    },
    {
      "bus": 5,
      "addresses": ["0x55"]
    }
  ]
}
```

Note that "bus" should be an unsigned integer and "addresses" be an array of
string of hex. Addresses can also be blocked on all buses. In this case, they
should be strings of hex, just like specifying an address on a bus. This is
useful for when you may not know the bus ahead of time, such as when adding
buses dynamically through the addition of an i2c-mux through a new_device node.

```json
{
  "addresses": ["0x70", "0x71"]
}
```

This example will not query addresses 0x70 or 0x71 on any bus.

## For both

```json
{
  "buses": [
    1,
    {
      "bus": 3,
      "addresses": ["0x30", "0x40"]
    }
  ],
  "addresses": ["0x71"]
}
```

This example will skip addresses 0x30 and 0x40 on bus 3, all devices on bus 1,
and all devices at address 0x71.
