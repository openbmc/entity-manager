###Introduction
There are two types of mux which are enabled in the system.
1. PCAxxx: These are enabled by a linux kernel driver.
2. gpioMux: These are enabled by gpio line.

##Problem:
Current EM doesn't have support for gpio based MUX and have limited support for
PCAxx mux with no support for nested mux.

I2C bus number for any devices behind muxes are not known until mux driver gets
instantiated. It is unknown at the time of writing configuration file and it
varies depending upon other i2c device detection.

##Solution(PCAxxx):
1. Either user predefine bus number in DTS file which will be hardcoded and can
only be used for one type of board for that platform. This means user will have
seperate DTS file for each different of a particular platform.

2. Dynamically detect these bus after initializing mux driver via entity manager
and it will provide a logical bus numbers via channels path under each mux. User
just need to configure type of device in each slot of mux and logical bus number
for those slots will be determined by EM.This will also support nested mux with
logical path.

Below is an example of i2c topology defined for multiple mux device along with
nested one.

i2c-$bus
|-- $bus-$address (FRU EEPROM detected)
`-- $bus-$MuxAddress (4-channel I2C MUX at MuxAddress)
    |-- i2c-60 (channel-0, PCIe slot 0)
    |-- i2c-68 (channel-1, PCIe slot 1)
    |   |-- 68-0040 (Temp sensor device)
    |   |-- 68-0070 (4-channel I2C MUX at 0x70)
    |   |   |-- i2c-70 (channel-0)
    |   |   |-- ...
    |   |   |-- ...
    |   |   `-- i2c-73 (channel-3)
    |   `-- 68-0072 (8-channel I2C MUX at 0x72)
    |       |-- i2c-81 (channel-0)
    |       |-- i2c-82 (channel-1)
    |       |-- ...
    |       |-- ...
    |       `-- i2c-88 (channel-7)
    |-- i2c-91 (channel-2, PCIe slot 2)
    `-- i2c-98 (channel-3)

Here $bus and $address are the bus number and address detected from FRU probe.
$MuxAddress is the address of Mux device defined by user. Naming syntax can
be redfined.

Following is entity manager configuration for above i2c topology. Entity manager
will create a logical json object for each device generated from logical i2c
device. logical i2c device numbers are derived from soft link through channel
directories under i2c mux.


"Exposes": [
    {
        "Address": "$address",
        "Bus": "$bus",
        "Name": "Riser 2 Fru",
        "Type": "EEPROM"
    },
    {
        "MuxAddress": "0x73",
        "Bus": "$bus",
    "Name": "Riser-2 Mux",
    "Type": "PCA9545Mux",
    "Channels": [
    {
        "Index": 0,
        "Name": "I2C-$bus Slot-$index",
        "Address": "0x40",
        "Type": "TMP421",
    },
    {
        "Index": 1,
        "Name": "I2C-$bus Mux-$MuxAddress Slot-$index Temp Sensor",
        "Address": "0x40",
        "Type": "TMP421",
    },
    {
        "Index": 1,
        "Name": "4-channel Mux",
        "Address": "0x70,
        "Type": "PCA9545Mux",
        "Channels": [
        {
            "Index": 0,
        "Name": "NVMe Temp Sensor",
        "Address": "0x60",
        "Type": "TMP421"
        },
        {..},
        {..},
        {
            "Index": 3
        "Name": "NVMe Temp Sensor",
        "Address": "0x80",
        "Type": "TMP421"
        }
        ]
    },
    {
        "Index": 1,
        "Name": "8-channel Mux",
        "Address": "0x70,
        "Type": "PCA9545Mux",
        "Channels": [
        {
            "Index": 0
        "Name": "NVMe Temp Sensor",
        "Address": "0x60",
        "Type": "TMP421"
        },
        {
            "Index": 1
        "Name": "NVMe Temp Sensor",
        "Address": "0x62",
        "Type": "TMP421"
        },
        {..},
        {..},
        {
            "Index": 7,
        "Name": "NVMe Temp Sensor",
        "Address": "0x80",
        "Type": "TMP421"
        }
        ]
    },
    {
        "Index": 2,
        "Name": "PCIe slot 2 temp sensor",
        "Address": "0x60
        "Type": "TMP421"
    },
    {
        "Index": 3,
        "Name": "PCIe slot 3",
    },

        ],
    }
]


##Solution(GPIO based):
Details for this features will added soon.

