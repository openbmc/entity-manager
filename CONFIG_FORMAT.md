# Config Files

## Intent

Configuration files are intended to represent the minimum amount of
configuration required to describe a given piece of hardware.  As such, they are
intended to be simple, and are guided by the following principles.

1. Configuration files should be easy to write.  If a tradeoff is to be made
   between a config file being complex to write, and a reactor being complex to
   write, the reactor will be the one to hold the complexity.  Why?
    - Configuration files will get replicated and built to support hundreds of
      systems over time, and scale linearly with the number of systems.  In
      contrast, reactors tend to scale as a logarithm of system count, with each
      new system supported adding fewer and fewer reactors.  As such, pushing
      the complexity to the reactors leads to fewer lines of code overall, even
      if the reactor itself contains more complex actions.
    - Reactor writers tend to be domain experts on their subsystem, and
      intimately understand the constraints that are emplaced on that subsystem.
      Config file writers are generally building support for a wide range of
      reactors on a single piece of hardware, and will generally have less
      knowledge of each individual reactors constraints.

2. Configuration files should trend toward one config file per physical piece
   of hardware, and should avoid attempting to support multiple variations of a
   given piece of hardware in a single file, even at the risk of duplicating
   information.  Why?
    - Hardware constraints, bugs, and oddities are generally found over time.
      The initial commit of a configuration file is far from the final time that
      changes will be submitted.  Having each individual piece of hardware in
      its own file minimizes the change needed between different components when
      bugs are found, or features are added.
    - Having separate config files reduces the number of platforms that need to
      be tested for any given config file change, thus limiting the "blast
      radius" of particular kinds of changes, as well as making an explicit log
      of what changed for a specific platform.
    - Having one config file per piece of hardware makes it much easier and
      clear for a user to determine if a piece of hardware is supported.
    - Note: This is a "guideline" not a "rule".  There are many cases of
      hardware that despite having different part numbers, are actually
      physically identical, and as such, the config files will never differ.
        - Example: SAS modules and cards made by the same company, on the same
          process, and branded with different manufacturers and part numbers.
        - Non-Example: Power supplies.  While all pmbus power supplies appear
          similar, there tend to be significant differences in featuresets,
          bugs, and OEM supported firmware features.  As such, they require
          separate config files.

3. Configuration files are not a long-term stable ABI.  Why?
    - Configuration files occasionally need to modify their schema in pursuit of
      simplicity, or based on a greater understanding of the system level
      constraints.
    - The repo will ensure that all schema changes are enacted such that the
      files in the repo will not be broken as a result of the schema change, and
      will be carried forward.  The recommended way to avoid merge problems is
      to upstream your configurations.
    - Note: This drives the requirement that config files shall not be checked
      into OpenBMC meta layers.

4. Configurations should represent only the things that are _different_ and
   undetectable between platforms.  Why?
    - There are many behaviors that the BMC has that are very easily detected
      at runtime, or where the behavior can be identical between different
      platforms.  Things like timeouts, protocol versions, and communcation
      channels can generally be represented with a default that works for all
      platforms, and doesn't need to be an entity-configurable parameter.  In
      general, reducing the config files to _only_ the differences reduces
      complexity, and explicitly bounds where dicsussion is needed for platform
      differences, and where a difference is "supported" and "reasonable" to
      maintain in the long run.

## Configuration Syntax

In most cases a server system is built with multiple hardware modules (circuit
boards) such as baseboard, risers, and hot-swap backplanes. While it is
perfectly legal to combine the JSON configuration information for all the
hardware modules into a single file if desired, it is recommended to divide them
into multiple configuration files. For example, there may be a baseboard JSON
file (describes all devices on the baseboard) and a chassis JSON file (describes
devices attached to the chassis).  Other examples of entities might be
removable power supplies, fans, and PCIe add in cards.

Within a configuration file, there is a JSON object which consists of multiple
"string : value" pairs. This Entity Manager defines the following strings.

| String    | Example Value                            | Description                              |
| :-------- | ---------------------------------------- | ---------------------------------------- |
| "Name"    | "X1000 1U Chassis"                       | Human readable name used for identification and sorting. |
| "Probe"   | "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME':'FFPANEL'})" | Statement which attempts to read from d-bus. The result determines if a configuration record should be applied. The value for probe can be set to “TRUE” in the case the record should always be applied, or set to more complex lookups, for instance a field in a FRU file that is exposed by the frudevice |
| "Exposes" | [{"Name" : "CPU fan"}, ...]              | An array of JSON objects which are valid if the probe result is successful. These objects describe the devices BMC can interact. |
| "Status"  | "disabled"                               | An indicator that allows for some records to be disabled by default. |
| "Bind*"  | "2U System Fan connector 1"              | The record isn't complete and needs to be combined with another to be functional. The value is a unique reference to a record elsewhere. |
| "DisableNode"| "Fan 1" | Sets the status of another Entity to disabled. |

Template strings in the form of "$identifier" may be used in configuration
files. The following table describes the template strings currently defined.

| Template String | Description                              |
| :-------------- | :--------------------------------------- |
| "$bus"          | During a I2C bus scan and when the "probe" command is successful, this template string is substituted with the bus number to which the device is connected. |
| "$address"   | When the "probe" is successful, this template string is substituted with the (7-bit) I2C address of the FRU device. |
| "$index"        | A run-tim enumeration. This template string is substituted with a unique index value when the "probe" command is successful. This allows multiple identical devices (e.g., HSBPs) to exist in a system but each with a unique name. |

## Configuration HowTos

If you're just getting started and your goal is to add sensors dynamically,
check out [My First Sensors](docs/my_first_sensors.md)

## Configuration schema

The config schema is documented in [README.md](schemas/README.schema)

## Configuration Records - Baseboard Example

The configuration JSON files attempt to model the actual hardware modules which
make up a complete system. An example baseboard JSON file shown below defines
two fan connectors and two temperature sensors of TMP75 type. These objects are
considered valid by BMC when the probe command (reads and compares the product
name in FRU) is successful and this baseboard is named as "WFP baseboard".

```
{
    "Exposes": [
        {
            "Name": "1U System Fan connector 1",
            "Pwm": 1,
            "Status": "disabled",
            "Tachs": [
                1,
                2
            ],
            "Type": "IntelFanConnector"
        },
        {
            "Name": "2U System Fan connector 1",
            "Pwm": 1,
            "Status": "disabled",
            "Tachs": [
                1
            ],
            "Type": "IntelFanConnector"
        },
        {
            "Address": "0x49",
            "Bus": 6,
            "Name": "Left Rear Temp",
            "Thresholds": [
                {
                    "Direction": "greater than",
                    "Name": "upper critical",
                    "Severity": 1,
                    "Value": 115
                },
                {
                    "Direction": "greater than",
                    "Name": "upper non critical",
                    "Severity": 0,
                    "Value": 110
                },
                {
                    "Direction": "less than",
                    "Name": "lower non critical",
                    "Severity": 0,
                    "Value": 5
                },
                {
                    "Direction": "less than",
                    "Name": "lower critical",
                    "Severity": 1,
                    "Value": 0
                }
            ],
            "Type": "TMP75"
        },
        {
            "Address": "0x48",
            "Bus": 6,
            "Name": "Voltage Regulator 1 Temp",
            "Thresholds": [
                {
                    "Direction": "greater than",
                    "Name": "upper critical",
                    "Severity": 1,
                    "Value": 115
                },
                {
                    "Direction": "greater than",
                    "Name": "upper non critical",
                    "Severity": 0,
                    "Value": 110
                },
                {
                    "Direction": "less than",
                    "Name": "lower non critical",
                    "Severity": 0,
                    "Value": 5
                },
                {
                    "Direction": "less than",
                    "Name": "lower critical",
                    "Severity": 1,
                    "Value": 0
                }
            ],
            "Type": "TMP75"
        }
    ],
    "Name": "WFP Baseboard",
    "Probe": "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME' : '.*WFT'})"
}
```

[Full Configuration](https://github.com/openbmc/entity-manager/blob/master/configurations/WFT_Baseboard.json)


#### Configuration Records - Chassis Example

Although fan connectors are considered a part of a baseboard, the physical fans
themselves are considered as a part of a chassis. In order for a fan to be
matched with a fan connector, the keyword "Bind" is used. The example below
shows how a chassis fan named "Fan 1" is connected to the connector named "1U
System Fan connector 1". When the probe command finds the correct product name
in baseboard FRU, the fan and the connector are considered as being joined
together.

```
{
    "Exposes": [
        {
            "BindConnector": "1U System Fan connector 1",
            "Name": "Fan 1",
            "Thresholds": [
                {
                    "Direction": "less than",
                    "Name": "lower critical",
                    "Severity": 1,
                    "Value": 1750
                },
                {
                    "Direction": "less than",
                    "Name": "lower non critical",
                    "Severity": 0,
                    "Value": 2000
                }
            ],
            "Type": "AspeedFan"
        }
    ]
}
```

## Enabling Sensors

As daemons can trigger off of shared types, sometimes some handshaking will be
needed to enable sensors. Using the TMP75 sensor as an example, when the sensor
object is enabled, the device tree must be updated before scanning may begin.
The entity-manager can key off of different types and export devices for
specific configurations. Once this is done, the baseboard temperature sensor
daemon can scan the sensors.

