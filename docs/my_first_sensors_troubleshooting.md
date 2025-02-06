# Troubleshooting sensors

This doc will describe components that need to be in place on a 'live' BMC
running OpenBMC in order for Entity-Manager and D-Bus sensors daemons to make
sensor readings avaliable on D-Bus (and by extension, redfish, WebUI, ect).

This document focuses on temp sensors that use the Hwmon API (very common), and
are handled through the HwmonTempSensor daemon from the D-Bus sensors repo.

Note that the doc @
https://github.com/openbmc/entity-manager/blob/master/docs/my_first_sensors.md
goes into much more detail about individual compoenents, and it's recommended
to read that first -- but some may find this troubleshooting/components
checklist helpful on its own.

## Troubleshooting temp sensors: a components checklist

1. D-Bus is working 
    * Service status can be confirmed by calling
        '> systemctl status dbus-broker.service' 
      or by calling
        '> busctl list' toshow avaliable D-Bus buses.

2. Device-compatible driver is included in image (typically part of the kernel)
    * If your device is compatible with a driver listed @ https://github.com/torvalds/linux/tree/master/drivers/hwmon
      you are likely okay here.

3. A valid Entity-Manager config file (.JSON format) that describes the device in
   one or more 'Exposes' records is in /usr/shared/entity-manager/configurations/
    * This directory is populated based on the contents on Enity-Manager repo's
      entity-manager/configurations/ at build time, with granular file inclusion
      controlled through a meson.build file)

5. Entity-Manager service is running
    * service status can be confirmed by calling
        '> systemctl status xyz.openbmc_project.entitymanager.service'

6. Probe statement for your device's Entity-Manager config is evaluated to TRUE
    * On a live system, Entity-Manager will evaluate probes in the JSON files in
      /usr/share/entity-manager/configurations at Entity-Manager service startup

    * Probes are also set to listen for changes on targeted D-Bus properties, so
      Entity-Manager can add or remove records dynamically based on changes as
      they happen

    * Probe statements commonly rely reading on targeted D-Bus properties, then
      matching values against them. Those properties are updated on D-Bus by
      VPD (Vital Product Data) daemons. See the hw_identification_daemons.md
      doc for more info.
     

7. The 'type' field in your device's 'Exposes' record in the JSON matches an
   item listed in 'I2CDeviceTypeMap sensorTypes' in HwmonTempSensorMain.cpp
   from the DBus-Sensors repo
    * Needed in order for D-Bus daemon-based driver bindings to work.
   
    * The SensorTypes map in HwmonTempSensorMain only handles for temp sensors
      bound to drivers that implement the Hwmon API (extremely common). Sensors
      that do not use the HwmonTempSensor API will have their maps in similar
      structures in their respective DBus-Sensor daemons (not HwmonTempSensor).

    * If your sensor is not included in the list, but the driver is already part
      of the kernel (see above), you may need to add it yourself -- which can be
      as simple asvcopy/pasting a line and adjusting the appropriate strings 

'''
    static const I2CDeviceTypeMap sensorTypes{
        {"ADM1021", I2CDeviceType{"adm1021", true}},
        {"DPS310", I2CDeviceType{"dps310", false}},
        {"EMC1403", I2CDeviceType{"emc1403", true}},
        {"EMC1412", I2CDeviceType{"emc1412", true}},

    Tip: The boolean field in I2CDeviceType is named 'createsHWMon'
'''

9. Device hardware works, and is powered on

    * Some devices are dependent on host power state, typically denoted by the
      'PowerState' field in the Entity-Manager Exposes record for a given device.
       Options are 'always' or 'on' (depends on host power) or 'ChassisOn'
      (depends on chasis power)
      
    * Devices that depend on host power may have special handling in D-Bus Sensors
      daemons (like HwmonTempSensor) to do things like add/remove devices from
      the i2c bus (and Hwmon sensor readings by extension)  as appropriate.
      
    * Changes to the host power state after HwmonTempSensor service startup
      completes can be recognized, and drivers bound to devices when that occurs.

10. Enityt-Manager 'exponses' records are showing up on D-Bus.
    *  D-Bus Daemons like HwmonTempSensor will read i2c bus, address, device
       'Type,' and 'PowerState' flag from entity-manager records on D-Bus
         a. The D-Bus Daemon (HwmonTempSensor, for example) will use sysfs's
            'add_device'to add a device of specified type and address to the
            i2c-bus called out in the exposes record.
       
         b. If device has already been added, attempt to bind a driver to it
            using 'bind_device' instead
       
    * Devices from the I2C bus are matched against Entity-Manager Exposes
      records on D-Bus by elements like their I2C bus and address.


12. HwmonTempSensor daemon, after confirming the new device is present on
    I2C bus and bound to a driver, will create records for each sensor:
    * '> busctl tree xyz.openbmc_project.HwmonTempSensor' should show a list of
      sensors underneath the parent bus.
    
      The output may look something like 

'''
     > busctl tree xyz.openbmc_project.HwmonTempSensor
      `- /xyz
        `- /xyz/openbmc_project
          `- /xyz/openbmc_project/sensors
            `- /xyz/openbmc_project/sensors/temperature
              |- /xyz/openbmc_project/sensors/temperature/Ambient_Temp
              |- /xyz/openbmc_project/sensors/temperature/Board_Temp
              |- /xyz/openbmc_project/sensors/temperature/Pcie_Zone_1_Temp
              |- /xyz/openbmc_project/sensors/temperature/PCIe_Zone_2_Temp
              `- /xyz/openbmc_project/sensors/temperature/Power_Zn
'''

    * You an check journal output for the daemon with
      '> journalctl -u xyz.openbmc_project.hwmontempsensor.service'
 
    * You can check status of the service with
      '> systemctl status xyz.openbmc_project.hwmontempsensor.service'