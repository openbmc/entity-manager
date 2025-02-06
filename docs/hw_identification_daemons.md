A list of 'Vital Product Data' (VPD) daemons hosted in OpenBMC.
This data can be used for hardware identification.

VPD daemons gather HW identifying information, and make it available on
D-Bus for consumption by Entity-Manager 'probe' evaluations (or
potentially by other BMC subsystems)

* FruDevice Daemon // reads FRU data from I2C-Based EEPROM (or a blob
  on filesystem) and makes it available on D-Bus
     * Design Doc: 
       * https://github.com/openbmc/entity-manager/blob/master/docs/my_first_sensors.md?#L67
         * (TODO: does anyone know of more specific documentation of the
           frudevice daemon?)
       * https://github.com/openbmc/docs/blob/master/designs/binarystore-via-blobs.md
         * TODO: I *think* this is the doc about adding the ability to
           read from blobs on filesystem instead of over I2C. Need to read closer
           to confirm

         * note: the community would generally prefer
           '1 VPD channel = 1 VPD daemon' -- so idealy, a filesystem
           blob reading daemon would be seperate from an I2C-reading
           daemon -- but this was allowed here for practicality
           reasons.

     * Source hosted in: Entity-Manager repo

     * D-Bus Bus: xyz.openbmc_project.FruDevice

     * D-Bus Interface: xyz.openbmc_project.FruDevice

     * Service status check: > systemctl xyz.openbmc_project.frudevice.service

     * Journal check: > journalctl -u xyz.openbmc_project.frudevice.service

     * D-Bus check: > busctl tree xyz.openbmc_project.FruDevice

     * Example EM Probe(s):
'''
       * "Probe": "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'F1UL16RISER\\d', 'ADDRESS' : 81})",

       * "Probe": [ "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'FFPANEL'})", "OR",
         "xyz.openbmc_project.FruDevice({'BOARD_PRODUCT_NAME': 'F2USTOPANEL'})"],
'''
* DeviceTree VPD Parser Daemon// reads data from device-tree nodes like 'model' or
                    'serial-number' and makes it available on D-Bus
    * Design Doc: 
      https://github.com/openbmc/docs/blob/master/designs/entity-manager-hw-id-vpd-dis
      cover-via-device-tree.md

    * Source hosted in: Entity-Manager

    * D-Bus Bus: xyz.openbmc_project.MachineContext

    * D-Bus Interface: xyz.openbmc_project.Inventory.Decorator.Asset

    * Service status check: > systemctl xyz.openbmc_project.machinecontext.service

    * Journal check: > journalctl -u xyz.openbmc_project.machinecontext.service

    * D-Bus check: > busctl tree xyz.openbmc_project.MachineContext

    * Example EM Probe(s):
'''
      * "Probe": ["xyz.openbmc_project.Inventory.Decorator.Asset({'Model': 'HPE-PCA-8675'})"],
'''

* OpenPowerVPD Parser Daemon // handles VPD that adheres to the OpenPowerVPD standard
    * Design Doc: 
      https://github.com/openbmc/docs/blob/master/designs/vpd-collection.md

    * Source hosted: https://github.com/openbmc/openpower-vpd-parser

    * D-Bus Bus: com.ibm.ipzvpd.VINI //TODO, need to confirm the correct 
                                       values...
    * D-Bus Interface: com.ibm.ipzvpd.VINI //TODO, need to confirm the correct 
      values...

     * Service status check: > systemctl xyz.openbmc_project.frudevice.service

     * Journal check: > journalctl -u xyz.openbmc_project.frudevice.service

     * D-Bus check: > busctl tree xyz.openbmc_project.FruDevice

    * Example EM Probe:
      * "Probe": [ "com.ibm.ipzvpd.VINI({'CC': [50, 69, 51, 51]})"],

* Honorable Mentions: 
  * Entity-Manager (doesn't directly gather VPD)

    * Entity-Manager probes can be set to key off other Entity-Manager 
      configurations (or combinations of them) by name.

      * Example EM Probe:
        * "Probe": "FOUND('IBM System1 Baseboard')",
          
* Some guidance on vendor-specific VPD daemons:
  * https://github.com/openbmc/docs/blob/master/hw-vendor-repos-policy.md#vendor-specific-feature
  * (TODO: is there any more documentation anyone is aware of re: where daemons 
    should live? I assume not)

* Some hastily added docs that come up in a search for "VPD" in 
  openbmc/docs (in addition to the ones linked above). 

    * Need to look these over more closely and decide how relevant they 
    actually are to this doc (ie, they talk about providing VPD data to d-bus,
    not just reading it off D-Bus)

  * https://github.com/openbmc/docs/blob/master/designs/psu-monitoring.md
  * https://github.com/openbmc/docs/blob/master/designs/nvmemi-over-smbus.md
  * https://github.com/openbmc/docs/blob/master/designs/voltage-regulator-configuration.md
  * https://github.com/openbmc/docs/blob/master/designs/bmc-boot-ready.md