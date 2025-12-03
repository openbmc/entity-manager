# Sensor-to-MUX Channel Mapping

This document is meant to bring you from nothing to using Entity-Manager with
Dbus-Sensors to populate a plug-in card's sensor values on dbus. When required,
a MUX such as PCA9548 can be used to connect sensors across multiple channels.
Once the sensor values are on dbus, they can be read via IPMI or Redfish. Doing
so is beyond the scope of this guide.

For this example, assume there is a PCIe card that exposes a 24c02 EEPROM,
TMP441 temperature sensor, and an INA219 power sensor. The PCIe slots are
connected through an SMBus multiplexer (PCA9548) on the motherboard and are
described in a device tree similar to the following:

```text
Ex : i2c1 (Master) --> Mux (PCA9548) --> channel-0 (24c02)
                                     --> channel-1 (tmp441)
                                     --> channel-2 (ina219)
```

We start with a simple hardware profile. We know that if the card's bus is
identified we know the address of the temperature sensor is 0x4c,address of adc
sensor is 0x2a and address of the eeprom is 0x50.

```text
i2c-1------- |--------------------|
             | PCA9548         ch0|----24c02 eeprom
             |                 ch1|----tmp441 temp
             |                 ch2|----ina219 adc
reset_pin----|--------------------|
```

```dts
aliases {
        i2c16 = &i2c_pe0;
        i2c17 = &i2c_pe1;
        i2c18 = &i2c_pe2;
        i2c19 = &i2c_pe3;
};

...

&i2c1 {
    status = "okay";
    i2c-switch@71 {
        compatible = "nxp,pca9546";
        reg = <0x71>;
        #address-cells = <1>;
        #size-cells = <0>;
        i2c-mux-idle-disconnect;

        i2c_pe0: i2c@0 {
            #address-cells = <1>;
            #size-cells = <0>;
            reg = <0>;

            eeprom@50 {
                compatible = "atmel,24c02";
                reg = <0x50>;
            };
        };
        i2c_pe1: i2c@1 {
            #address-cells = <1>;
            #size-cells = <0>;
            reg = <1>;

            tmp421@4c {
                compatible = "ti,tmp421";
                reg = <0x4c>;
            };
        };
        i2c_pe2: i2c@2 {
            #address-cells = <1>;
            #size-cells = <0>;
            reg = <2>;

            adc@2a {
                compatible = "ti,ina219";
                reg = <0x2a>;
            };
        };
        i2c_pe3: i2c@3 {
            #address-cells = <1>;
            #size-cells = <0>;
            reg = <3>;
        };
    };
};
```

Entity-Manager provides a very powerful mechanism for querying various
information, but our goal is simple. If we find the card, we want to add
the device to the system and tell dbus-sensors that there is a hwmon temperature
and adc sensors available.

We start with a simple hardware profile. We know that if the card's bus is
identified we know the address of the temperature sensor is 0x4c and address of
adc sensor is 0x2a.

```json
{
  "Exposes": [
    {
      "Address": "$address",
      "Bus": "$bus",
      "Name": "$bus great eeprom",
      "Type": "EEPROM_24C02"
    },
    {
      "Address": "0x4c>",
      "Bus": "$bus+1",
      "Name": "$bus great local",
      "Type": "TMP441"
    },
    {
      "Address": "0x2a",
      "Bus": "$bus+2",
      "Name": "$bus great local1",
      "Type": "ADC"
    }
  ],
  "Name": "$bus Great Card",
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
`-/xyz
  `-/xyz/openbmc_project
    `-/xyz/openbmc_project/sensors
      `-/xyz/openbmc_project/sensors/temperature
        |-/xyz/openbmc_project/sensors/temperature/18_great_local
        |-/xyz/openbmc_project/sensors/temperature/18_great_ext
        |-/xyz/openbmc_project/sensors/temperature/19_great_local
        |-/xyz/openbmc_project/sensors/temperature/19_great_ext

~# busctl introspect --no-pager xyz.openbmc_project.HwmonTempSensor \
 /xyz/openbmc_project/sensors/temperature/18_great_local

NAME                                TYPE      SIGNATURE RESULT/VALUE                             FLAGS
org.freedesktop.DBus.Introspectable interface -         -                                        -
.Introspect                         method    -         s                                        -
org.freedesktop.DBus.Peer           interface -         -                                        -
.GetMachineId                       method    -         s                                        -
.Ping                               method    -         -                                        -
org.freedesktop.DBus.Properties     interface -         -                                        -
.Get                                method    ss        v                                        -
.GetAll                             method    s         a{sv}                                    -
.Set                                method    ssv       -                                        -
.PropertiesChanged                  signal    sa{sv}as  -                                        -
org.openbmc.Associations            interface -         -                                        -
.associations                       property  a(sss)    1 "chassis" "all_sensors" "/xyz/openb... emits-change
xyz.openbmc_project.Sensor.Value    interface -         -                                        -
.MaxValue                           property  d         127                                      emits-change
.MinValue                           property  d         -128                                     emits-change
.Value                              property  d         31.938                                   emits-change writable

```

There you are! You now have the three sensors from the two card instances on
dbus.

This can be more complex, for instance if your card has a mux you can add it to
the configuration, which will trigger FruDevice to scan those new buses for more
devices.
