# Dynamic I²C Mux Configuration

Author: Vijay Khemka (vijay)

Primary assignee: Vijay Khemka (vijay)

Other contributors:

Created: May 06, 2021

## Problem Description
Current EM doesn't have support for gpio switch based MUX and have limited
support for PCAxx type mux and nested mux.

I²C bus number for any devices behind muxes are not known until mux driver gets
instantiated. It is unknown at the time of writing configuration file and it
varies depending upon other I²C device detection.

## Background and References
Many platforms needs to support multiple PCIe cards that have I²C muxes on
them. Entity-manager's design assumes that I²C devices have a FRU EEPROM
identifying muxes on that same I²C bus but it is not true always, specially
for expansion cards that have an I²C mux with one FRU EEPROM and many I²C
channels.

There are several types of I²C mux used in the system and needed to be detected
dynamically for different boards based on configuration. Following types of
mux/switch will be covered here.
1. PCAxxx: These are enabled by a linux kernel driver.
2. gpioSwitch: These are enabled by gpio line.

## Requirements
Many platform has several boards with little change in hardware like extra
riser card or dynamically plug in devices. To serve all these boards with
single image, a run time detection of hardware type needs to be supported
and these need to be configured accordingly.

## Solution (PCAxxx)
Dynamically detect I²C bus after initializing mux driver via entity manager
and it will provide a logical bus numbers via channels path under each mux. User
just need to configure type of device in each slot of mux and logical bus number
for those slots will be determined by EM.This will also support nested mux with
logical path.

Below is an example of I²C topology defined for multiple mux device along with
nested one.

i2c-$bus
|-- $bus-$address (FRU EEPROM detected)
|-- $bus-$MuxAddress (4-channel I2C MUX at MuxAddress)
>    |-- i2c-60 (channel-0, PCIe slot 0)
>    |-- i2c-68 (channel-1, PCIe slot 1)
>>     |-- 68-0040 (Temp sensor device)
>>     |-- 68-0070 (4-channel I2C MUX at 0x70)
>>>     |-- i2c-70 (channel-0)
>>>     |-- ...
>>>     |-- ...
>>>     `-- i2c-73 (channel-3)
>>     |-- 68-0072 (8-channel I2C MUX at 0x72)
>>>     |-- i2c-81 (channel-0)
>>>     |-- i2c-82 (channel-1)
>>>     |-- ...
>>>     |-- ...
>>>     `-- i2c-88 (channel-7)
>    |-- i2c-91 (channel-2, PCIe slot 2)
>    `-- i2c-98 (channel-3)

Here $bus and $address are the bus number and address detected from FRU probe.
$MuxAddress is the address of Mux device defined by user. Naming syntax can
be redfined.

Following is entity manager configuration for above I²C topology. Entity manager
will create a logical json object for each device generated from logical I²C
device. logical I²C device numbers are derived from soft link through channel
directories under I²C mux.

"Exposes": [
    {
        "Address": "$address",
        "Bus": "$bus",
        "Name": "Riser 2 Fru",
        "Type": "EEPROM"
    },
    {
        "Address": "0x73",
        "Bus": "$bus",
        "Name": "Riser-2 Mux",
        "Type": "PCA9545Mux",
        "ChannelNames": [
            "Slot_0",
            "Slot_1",
            "Slot_2",
            "Slot_3"
        ]
    },
    {
        "Address": "0x40",
        "Bus": "$Slot_0",
        "Name": "Ambient 0 Temp",
        "Type": "TMP421"
    },
    {
        "Address": "0x40",
        "Bus": "$Slot_1",
        "Name": "Ambient 1 Temp",
        "Type": "TMP421"
    },
    {
        "Address": "0x70",
        "Bus": "$Slot_1",
        "Name": "Slot-1 4 Channel Mux",
        "Type": "PCA9545Mux",
        "ChannelNames": [
            "Slot_1_0",
            "Slot_1_1",
            "Slot_1_2",
            "Slot_1_3"
        ]
    },
    {
        "Address": "0x60",
        "Bus": "$Slot_1_0",
        "Name": "Slot 1 Channel 0 Temp",
        "Type": "TMP421"
    },
    {
        "Address": "0x80",
        "Bus": "$Slot_1_3",
        "Name": "Slot 1 Channel 3 Temp",
        "Type": "TMP421"
    },
    {
        "Address": "0x72",
        "Bus": "$Slot_1",
        "Name": "Slot-1 8 Channel Mux",
        "Type": "PCA9547Mux",
        "ChannelNames": [
            "Slot_1_0",
            "Slot_1_1",
            "Slot_1_2",
            "Slot_1_3",
            "Slot_1_4",
            "Slot_1_5",
            "Slot_1_6",
            "Slot_1_7"
        ]
    },
    {
        "Address": "0x60",
        "Bus": "$Slot_1_0",
        "Name": "Slot 1 Channel 0 Temp",
        "Type": "TMP421"
    },
    {
        "Address": "0x62",
        "Bus": "$Slot_1_1",
        "Name": "Slot 1 Channel 1 Temp",
        "Type": "TMP421"
    },
    {
        "Address": "0x80",
        "Bus": "$Slot_1_7",
        "Name": "Slot 1 Channel 7 Temp",
        "Type": "TMP421"
    },
    {
        "Address": "0x60",
        "Bus": "$Slot_2",
        "Name": "Slot 2 Temp",
        "Type": "TMP421"
    }
]

## Solution (GPIO switch):
There is Gpio switch which are enabled through gpio lines and each channels are
identified through set of gpio lines. These lines can be defined as an array of
gpio nodes with high or low to detect a particular channels and configure them.

Let's assume there are two GPIO line Q4 and Q5 are used to address each slots on
the switch from 0,3. Then each device needs to define these gpio lines polarity
to enable it's slot. Current design will support only single port selection in
single configuration so it selects particular line and the all device becomes
visible and all the kernel driver can be initialized via loadOverlays API.

An advance implementation can support multiple selection of ports which need a
binding and unbinding of devices at every selection.

This is not for gpio based mux which needs i2c-mux-gpio kernel driver. This
only cover gpio based switch like PI5C3253 defined in link
https://www.diodes.com/assets/Datasheets/PI5C3253.pdf

Below is the example for EM configuration

    {
        "Bus": 6,
        "Name": "GPIO Slot 1 Temp",
        "Address": "0x60,
        "Type": "TMP421",
        "BridgeGpio": [
            {
                "Name": "MUX_BRIDGE_Q4_EN",
                "Polarity": "Low"
            },
            {
                "Name": "MUX_BRIDGE_Q5_EN",
                "Polarity": "High"
            }
        ]
    },
    {
        "Bus": 6,
        "Name": "GPIO Slot 3 Temp",
        "Address": "0x62",
        "Type": "TMP421",
        "BridgeGpio": [
            {
                "Name": "MUX_BRIDGE_Q4_EN",
                "Polarity": "High"
            },
            {
                "Name": "MUX_BRIDGE_Q5_EN",
                "Polarity": "Low"
            }
        ]
    }

GPIO line name like MUX_BRIDGE_Q5_EN must be defined in kernel to be enabled
via libgpiod.

## Alternatives Considered
User will predefine bus number in DTS file which will be hardcoded and can
only be used for one type of board for that platform. This means user will have
seperate DTS file for each different board of a particular platform and muxes
on expansion cards can't be used, as expansion cards won't be known and could
be connected runtime and detected dynamically. That's not going to meet the
overall needs.

## Impacts
Minimal, it will add multiple layers of enumeration, it happens only few times
when card is inserted and at the start. Once enumeration completes it will
create a static node for each device which can be used by other daemons.

## Testing
This will be tested with a platform having expansion card with mux connected.
Configuration file will define relationship between mux and baseboard I²C bus.
Code should find logical bus number and create a dbus node along with bus number
and device address details.
