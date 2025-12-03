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

An example Entity-Manager configuration using a PCA9548 I2C multiplexer and
downstream temperature sensors can be found here:

[2]:
  https://github.com/openbmc/entity-manager/blob/master/configurations/meta/anacapa/anacapa_bridge_r.json

This configuration shows how mux channels create additional I2C buses that are
used by dbus-sensors to expose hwmon temperature sensors on D-Bus.

The dbus-sensors suite of daemons each run searching for a specific type of
sensor. In this case the hwmon temperature sensor daemon will recognize the
configuration interface: xyz.openbmc_project.Configuration.TMP441.

It will look up the device on i2c and see there is a hwmon instance, and map
temp1_input to Name and since there is also Name1 it'll map temp2_input.

```sh
~# busctl tree --no-pager xyz.openbmc_project.HwmonTempSensor

~# busctl introspect --no-pager xyz.openbmc_project.HwmonTempSensor \
 /xyz/openbmc_project/sensors/temperature/tmp_441
 /xyz/openbmc_project/sensors/temperature/max_31725

```

There you are! You now have the sensors exposed on D-Bus through dbus-sensors.

This can be extended further by adding additional devices or multiplexers to the
configuration, allowing Entity-Manager and dbus-sensors to discover and populate
sensors on newly exposed I2C buses.
