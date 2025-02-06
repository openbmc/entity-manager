A list of 'Vital Product Data' (VPD) daemons hosted in OpenBMC. This data can be used for hardware identification.

VPD daemons gather HW identifying information, and make it avaliable on D-Bus for consumption by Entity-Manager 'probe' evaluations
(or potentially by other BMC subsystems)

* FruDevice // source hosted in Entity-Manager repo, pulls FRU data from I2C-Based EEPROM and makes it avaliable on D-Bus's 'xyz.openbmc_project.FruDevice' bus using interface

* DeviceTree-VPD // source hosted in Entity-Manager repo, pulls data from device-tree nodes like 'model' or 'serial-number' and makes it avaliable on D-Bus's 'xyz.openbmc_project.MachineContext' bus using interface 'xyz.openbmc_project.Inventory.Decorator.Asset' 

* OpenPowerVPD // hosted in openpower-vpd-parser repo #TODO: (need to dig up more details)
