#Sensor - to - MUX Channel Mapping

This document is meant to bring you from nothing to using Entity-Manager
with dbus-sensors to populate a plug-in card's sensor values on dbus. When
required, a MUX such as PCA9548 can be used to connect sensors across
multiple channels. Once the sensor values are on dbus, they can be read via
IPMI or Redfish. Doing so is beyond the scope of this guide.

For this example, assume there is a PCIe card that exposes a 24C02 EEPROM,
TMP441 temperature sensor, and an INA219 power sensor. The PCIe slots are
connected through an SMBus multiplexer (PCA9548) on the motherboard.

```text
[BMC]
Ex : i2c1 (Master) --> Mux (PCA9548) --> Connector (J1) --> channel-0 (24c02)
                                                        --> channel-1 (tmp441)
                                                        --> channel-2 (ina219)
```

A simple hardware card is used. Once the card bus is identified, the temperature
sensor address is 0x4C, the ADC sensor address is 0x2A, and the EEPROM address
is 0x50.

```text
i2c-1------- |--------------------|
             | PCA9548         ch0|----24c02 eeprom
             |                 ch1|----tmp441 temp
             |                 ch2|----ina219 adc
reset_pin----|--------------------|
```

Linux kernel DT binding example for PCA9548 I2C mux [1]

[1]: https://github.com/openbmc/linux/blob/dev-6.12/Documentation/devicetree/bindings/i2c/i2c-mux-pca954x.yaml

If the card is detected, the device is added to the system, and dbus-sensors
is notified that hardware monitoring temperature and ADC sensors are available.

A simple hardware profile is used. Once the card bus is identified, the
temperature sensor address is 0x4C, and the ADC sensor address is 0x2A.

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
        "ChannelNames": [
                "Pcie_Slot_1",
                "Pcie_Slot_2",
                "Pcie_Slot_3",
                ""
            ],
            "Name": "Great Mux",
            "Type": "PCA9545Mux"
        },
    {
      "Address": "0x4c",
      "Bus": "$bus+1",
      "Name": "temparature great local",
      "Type": "TMP441"
    },
    {
      "Address": "0x2a",
      "Bus": "$bus+2",
      "Name": "adc great local",
      "Type": "ADC"
    }
  ],
  "Name": "Great Card",
  "Probe": "xyz.openbmc_project.FruDevice({'PRODUCT_PRODUCT_NAME': 'Super Great'})",
  "Type": "Board"
}
```

The dbus-sensors suite of daemons each run searching for a specific type of
sensor. In this case the hwmon temperature sensor daemon will recognize the
configuration interface: `xyz.openbmc_project.Configuration.TMP441`.

It will look up the device on i2c and see there is a hwmon instance, and map
`temp1_input` to `Name` and since there is also `Name1` it'll map `temp2_input`.

```sh
~# busctl tree --no-pager xyz.openbmc_project.HwmonTempSensor

~# busctl introspect --no-pager xyz.openbmc_project.HwmonTempSensor \
 /xyz/openbmc_project/sensors/temperature/18_great_local

```

There you are! You now have the three sensors from the two card instances on
dbus.

This can be more complex, for instance if your card has a mux you can add it to
the configuration, which will trigger FruDevice to scan those new buses for more
devices.
