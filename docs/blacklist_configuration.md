# Blacklist Configuration
The blacklist.json in package directory can determine i2c buses and addresses that don't need to be scan by FruDevice.
## For buses
Put in numbers of buses. For example:
```json
{
    "buses": [1, 3, 5]
}
```
Note that "buses" should be an array.
## For addresses
Put in bus and addresses with this format:
```json
{
    "buses": [
        {
            "bus": 3,
            "addresses": [
                "0x30"
                "0x40"
            ]
        },
        {
            "bus": 5,
            "addresses": [
                "0x55"
            ]
        }
    ]
}
```
Note that "bus" should be an unsigned integer and "addresses" be an array of string.
## For both
```json
{
    "buses": [
        1,
        {
            "bus": 3,
            "addresses": [
                "0x30"
                "0x40"
            ]
        }
    ]
}
```
