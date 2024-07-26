# Fru Device EEPROM Write Support

The FruDevice service uses EEPROMs conforming to the
[FRU Information Storage Definition](https://www.intel.com/content/dam/www/public/us/en/documents/specification-updates/ipmi-platform-mgt-fru-info-storage-def-v1-0-rev-1-3-spec-update.pdf)
to detect components of a system.

Often there is the desire to update or adjust some parts of the contained
information. For this reason,
[ipmid](https://github.com/openbmc/phosphor-host-ipmid/) already has limited
support for editing fields.

## Proposed Design

The proposed design aims to implement FRU write support in FruDevice directly,
and be able to update any field without pulling in parts of the ipmi stack,
which many consider as legacy and do not want to deploy to their systems.
However they still rely on the eeprom contents as per the spec.

The implementation will not try to adjust the contents in-place but generate the
complete contents anew, based on

- the extracted fields as detected by FruDevice
- the updated field

This has the advantage that there is almost no limitation in terms of not being
able to expand field sizes. Any field may be added or updated.

Then [frugen](https://codeberg.org/IPMITool/frugen) will be used to generate the
new contents.

## D-bus interface

**TODO**

The idea here is to have a `writeFruProperty` function that either accepts bus
and address, or just exists directly on the object path of the eeprom in
question.

So `busctl call ... writeFruProperty uuss $bus $addr $property $value` or
`busctl call ... writeFruProperty ss $property $value`

The advantage of supplying the bus and address are that even the string that
FruDevice uses to form the object path could be overwritten and the dbus method
would still work afterwards.

The advantage of not supplying bus and address is that it's more ergonomic to
use as bus and address can be arbitrary and first have to be retrieved. Also
it's unlikely to edit the properties used for `PROBE` .

## Backup and Restore

Since FRU contents often contain critical information about the system, the
complete eeprom contents will be backed up to
`/var/backup/eeprom/backup_${BUS}_${ADDR}.bin` before any write is attempted.

Restore can be done by manually overwriting the eeprom with the backup.

## References

FRU Information Storage Definition
https://www.intel.com/content/dam/www/public/us/en/documents/specification-updates/ipmi-platform-mgt-fru-info-storage-def-v1-0-rev-1-3-spec-update.pdf

frugen https://codeberg.org/IPMITool/frugen
