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
string of hex.

## For no_block

The no_block key allows specifying addresses on a bus that should only be
scanned, blocking all other addresses. This provides a whitelist-based approach.
For example:

```json
{
  "buses": [
    {
      "bus": 9,
      "no_block": ["0x50", "0x54"]
    }
  ]
}
```

In this example, only addresses 0x50 and 0x54 on bus 9 will be scanned, while
all other addresses on the same bus will be blocked.

Note that "no_block" should be an array of strings in hex format

## For both

```json
{
  "buses": [
    1,
    {
      "bus": 3,
      "addresses": ["0x30", "0x40"]
    },
    {
      "bus": 9,
      "no_block": ["0x50", "0x54"]
    }
  ]
}
```
