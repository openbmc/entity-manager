#Entity Manager#

Entity manager is a runtime configuration application which parses configuration
files (in JSON format) and attempts to detect the devices described by the
configuration files. It also can, based on the configuration, attempt to load
device tree overlays to add sensors to the device tree. The output is also a
JSON file which includes all devices in the system such as fans and temperature
sensors.

## Configuration Syntax ##

In most cases a server system is built with multiple hardware modules (circuit
boards) such as baseboard, risers, and hot-swap backplanes. While it is
perfectly legal to combine the JSON configuration information for all the
hardware modules into a single file if desired, it is also possible to divide
them into multilple configuration files. For example, there may be a baseboard
JSON file (describes all devices on the baseboard) and a chassis JSON file
(describes devices attached to the chassis). When one of the hardware modules
needs to be upgraded (e.g., a new temperature sensor), only such JSON
configuration file needs to be be updated.

Within a configuration file, there is a JSON object which consists of
multiple "string : value" pairs. This Entity Manager defines the following
strings.

| String    | Example Value                            | Description                              |
| :-------- | ---------------------------------------- | ---------------------------------------- |
| "name"    | "X1000 1U Chassis"                       | Human readable name used for identification and sorting. |
| "probe"   | "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME':'FFPANEL'})" | Statement which attempts to read from the hardware. The result determines if a configuration record should be applied. The value for probe can be set to “TRUE” in the case the record should always be applied, or set to more complex lookups, for instance a field in a FRU file. |
| "exposes" | [{"name" : "CPU fan"}, ...]              | An array of JSON objects which are valid if the probe result is successful. These objects describe the devices BMC can interact. |
| "status"  | "disabled"                               | An indicator that allows for some records to be disabled by default. |
| "bind_*"  | "2U System Fan connector 1"              | The record isn't complete and needs to be combined with another to be functional. The value is a unique reference to a record elsewhere. |

Template strings in the form of "$identifier" may be used in configuration
files. The following table describes the template strings currently defined.

| Template String | Description                              |
| :-------------- | :--------------------------------------- |
| "$bus"          | During a I2C bus scan and when the "probe" command is successful, this template string is substituted with the bus number to which the device is connected. |
| "$address"   | When the "probe" is successful, this template string is substituted with the (7-bit) I2C address of the FRU device. |
| "$index"        | A run-tim enumeration. This template string is substituted with a unique index value when the "probe" command is successful. This allows multiple identical devices (e.g., HSBPs) to exist in a system but each with a unique name. |



## Configuration Records - Baseboard Example##

Required fields are name, probe and exposes.

The configuration JSON files attempt to model after actual hardware modules
which made up a complete system. An example baseboard JSON file shown below
defines two fan connectors and two temperature sensors of TMP75 type. These
objects are considered valid by BMC when the probe command (reads and
compares the product name in FRU) is successful and this baseboard is named
as "WFP baseboard".

```
{
    "exposes": [
        {
            "name": "1U System Fan connector 1",
            "pwm": 1,
            "status": "disabled",
            "tachs": [
                1,
                2
            ],
            "type": "IntelFanConnector"
        },
        {
            "name": "2U System Fan connector 1",
            "pwm": 1,
            "status": "disabled",
            "tachs": [
                1
            ],
            "type": "IntelFanConnector"
        },
        {
            "address": "0x49",
            "bus": 6,
            "name": "Left Rear Temp",
            "thresholds": [
                [
                    {
                        "direction": "greater than",
                        "name": "upper critical",
                        "severity": 1,
                        "value": 115
                    },
                    {
                        "direction": "greater than",
                        "name": "upper non critical",
                        "severity": 0,
                        "value": 110
                    },
                    {
                        "direction": "less than",
                        "name": "lower non critical",
                        "severity": 0,
                        "value": 5
                    },
                    {
                        "direction": "less than",
                        "name": "lower critical",
                        "severity": 1,
                        "value": 0
                    }
                ]
            ],
            "type": "TMP75"
        },
        {
            "address": "0x48",
            "bus": 6,
            "name": "Voltage Regulator 1 Temp",
            "thresholds": [
                [
                    {
                        "direction": "greater than",
                        "name": "upper critical",
                        "severity": 1,
                        "value": 115
                    },
                    {
                        "direction": "greater than",
                        "name": "upper non critical",
                        "severity": 0,
                        "value": 110
                    },
                    {
                        "direction": "less than",
                        "name": "lower non critical",
                        "severity": 0,
                        "value": 5
                    },
                    {
                        "direction": "less than",
                        "name": "lower critical",
                        "severity": 1,
                        "value": 0
                    }
                ]
            ],
            "type": "TMP75"
        }
    ],
    "name": "WFP Baseboard",
    "probe": "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME' : '.*WFT'})"
}
```

####Configuration Records - Chassis Example ####

Although fan connectors are considered a part of a baseboard, the physical
fans themselves are considered as a part of a chassis. In order for a fan to
be matched with a fan connector, the keyword "bind_connector" is used. The
example below shows how a chassis fan named "Fan 1" is connected to the
connector named "1U System Fan connector 1". When the probe command finds the
correct product name in baseboard FRU, the fan and the connector are
considered as being joined together.

```
{
    "exposes": [
        {
            "bind_connector": "1U System Fan connector 1",
            "name": "Fan 1",
            "thresholds": [
                [
                    {
                        "direction": "less than",
                        "name": "lower critical",
                        "severity": 1,
                        "value": 1750
                    },
                    {
                        "direction": "less than",
                        "name": "lower non critical",
                        "severity": 0,
                        "value": 2000
                    }
                ]
            ],
            "type": "AspeedFan"
        }
    ]
}
```

## Enabling Sensors ##

As demons can trigger off of shared types, sometimes some handshaking will be
needed to enable sensors. Using the TMP75 sensor as an example, when the
sensor object is enabled, the device tree must be updated before scanning may
begin. The device tree overlay generator has the ability to key off of
different types and create device tree overlays for specific offsets. Once
this is done, the baseboard temperature sensor demon can scan the sensors.

## Run Unit Tests ##

The following environment variables need to be set to run unit tests:

* TEST: 1, this disables the fru parser from scanning on init and changes the
work directories.
