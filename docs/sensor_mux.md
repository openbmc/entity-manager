# Sensor-to-MUX Channel Map

This document guides the use of Entity-Manager with dbus-sensors to populate
sensor values on dbus. When required, a MUX such as PCA9548 can be used to
connect sensors across multiple channels.

For this example, an I2C bus is connected to a PCA9548 multiplexer. Each mux
channel exposes a single downstream device: 24C02 EEPROM, TMP441 temperature
sensor, and an MAX31725 sensor.

```text
[BMC]
i2c1 (Master)
     │
     ▼
┌─────────────┐
│ PCA9548 Mux │
│             │──► channel-0 ──► 24c02     (0x50)
│             │──► channel-1 ──► tmp441    (0x4c)
│             │──► channel-2 ──► max31725  (0x2a)
└─────────────┘
```

Linux kernel DT binding example for PCA9548 I2C mux [1]

[1]:
  https://github.com/openbmc/linux/blob/dev-6.12/Documentation/devicetree/bindings/i2c/i2c-mux-pca954x.yaml

If the device is detected, it is added to the system and dbus-sensors is
notified that hardware monitoring temperature and other sensors are available.

```json
{
  "Exposes": [
    {
      "Address": "$address",
      "Bus": "$bus",
      "Name": "great eeprom",
      "Type": "EEPROM_24C02"
    },
    {
      "Address": "0x72",
      "Bus": "$bus",
      "ChannelNames": ["Channel_0", "Channel_1", "Channel_2"],
      "Name": "Great Mux",
      "Type": "PCA9545Mux"
    },
    {
      "Address": "0x4C",
      "Bus": "$bus+1",
      "Name": "Temperature Great Local",
      "Type": "TMP441"
    },
    {
      "Address": "0x2A",
      "Bus": "$bus+2",
      "Name": "INLET_T Great Local",
      "Type": "MAX31725"
    }
  ],
  "Name": "Great Device",
  "Probe": "xyz.openbmc_project.FruDevice({'PRODUCT_PRODUCT_NAME': 'Super Great'})",
  "Type": "Board"
}
```

The dbus-sensors suite of daemons each run searching for a specific type of
sensor. In this case the hwmon temperature sensor daemon will recognize the
configuration interface: xyz.openbmc_project.Configuration.TMP441.

It will look up the device on i2c and see there is a hwmon instance, and map
temp1_input to Name and since there is also Name1 it'll map temp2_input.

```sh
~# busctl tree --no-pager xyz.openbmc_project.HwmonTempSensor

~# busctl introspect --no-pager xyz.openbmc_project.HwmonTempSensor \
 /xyz/openbmc_project/sensors/temperature/18_great_local

```

There you are! You now have the sensors exposed on D-Bus through dbus-sensors.

This can be extended further by adding additional devices or multiplexers to the
configuration, allowing Entity-Manager and dbus-sensors to discover and populate
sensors on newly exposed I2C buses.
