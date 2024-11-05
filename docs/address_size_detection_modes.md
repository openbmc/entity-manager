# EEPROM address size detection modes

This document introduces and discusses the different modes to detect how many
address byte(s) needed for a given EEPROM device.

## MODE-1

The existing upstream function isDevice16Bit() bases on sending 1-byte write
operation (with a STOP condition) and 8 subsequent 1-byte read operations with
SINGLE byte address.

### This MODE-1 expects the following logic

- If the device requires 1 address byte, it EXPECTS that the data will be read
  from a single location so 8 bytes read will be the same.
- If the device requires 2 address bytes, it EXPECTS that the data will be read
  from 8 DIFFERENT LOCATIONS and at least one byte read is different than 7
  other reads.

### Issue and potential issue with this MODE-1

- If any "2 address bytes" EEPROM from any vendor has the same data in all
  memory locations (0-7) the existing upstream function read, this device will
  be identified as "1 address byte" device.

- ONSEMI EEPROM (a 2 address bytes device) return the same data from the same
  single byte address read --> therefore, existing function wrongly identifies
  it as 1 byte address device.

## MODE-2

The proposal MODE-2 changes to isDevice16Bit() sends 8 instructions of 2-bytes
write operation (WITHOUT a STOP condition ie. prohibited STOP) followed by a
1-byte read operation. The proposed solution fully complies with IIC standard
and should be applicable to any IIC EEPROM manufacturer.

```text
`| Start | SlaveAddr + W | 0x00 | 0x00 | STOP PROHIBITED HERE | Start | SlaveAddr + R | data byte | Stop |
`|-------|---------------|------|------|----------------------|-------| --------------|-----------|------|
`| Start | SlaveAddr + W | 0x00 | 0x01 | STOP PROHIBITED HERE | Start | SlaveAddr + R | data byte | Stop |
`| Start | SlaveAddr + W | 0x00 | 0x02 | STOP PROHIBITED HERE | Start | SlaveAddr + R | data byte | Stop |
`| Start | SlaveAddr + W | 0x00 | 0x03 | STOP PROHIBITED HERE | Start | SlaveAddr + R | data byte | Stop |
`| Start | SlaveAddr + W | 0x00 | 0x04 | STOP PROHIBITED HERE | Start | SlaveAddr + R | data byte | Stop |
`| Start | SlaveAddr + W | 0x00 | 0x05 | STOP PROHIBITED HERE | Start | SlaveAddr + R | data byte | Stop |
`| Start | SlaveAddr + W | 0x00 | 0x06 | STOP PROHIBITED HERE | Start | SlaveAddr + R | data byte | Stop |
`| Start | SlaveAddr + W | 0x00 | 0x07 | STOP PROHIBITED HERE | Start | SlaveAddr + R | data byte | Stop |
```

- If the device requires a single data byte, then it will always load address
  0x00, the subsequent read byte will be the same for all 8 instructions. The
  second byte on the write would be interpreted as data byte, thus not modifying
  the address pointer.

- If two address bytes are required, then the device will interpret both bytes
  as addresses, thus reading from different addresses every time, similar with
  what the existing function is using now.

### Notes & reasons

There is no STOP condition after the second (potential) address byte. A START
condition must be sent after the second byte. If STOP condition is sent, then
the 1-byte address devices will start internal write cycle, altering the EEPROM
content which is not good.

This proposal MODE-2 suffers the same 1st issue as MODE-1 ie. what if the EEPROM
has the same data at all those addresses. However, this proposal MODE-2
addresses the 2nd issue of MODE-1 which expects that the data will be read from
8 DIFFERENT LOCATIONS if the device requires 2 address bytes. This expectation
is the ambiguity (not standard defined) in the IIC specification.

In [IIC specification:](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)

- Section 3.1.10, Note 2 ->
  `All decisions on auto-increment or decrement of previously accessed memory locations, etc., are taken by the designer of the device.`

  Based on this note, the designer of every EEPROM has the "freedom" to use
  whatever architecture considers appropriate and suitable to process everyone
  of the two address bytes. There are no restrictions on this.

  Based on this, the others EEPROM (not ONSEMI EEPROM) auto-increment - observed
  with one address byte sent instead of two - is a manufacturer-specific
  behavior, and not standard defined.

- Section 3.1.10, Note 1 ->
  `Combined formats can be used, for example, to control a serial memory. The internal memory location must be written during the first data byte. After the START condition and slave address is repeated, data can be transferred.`

  This proposal MODE-2 implements this note. The memory location referred herein
  is the address pointer, as being the first data byte in IIC communication.
  Based on this note, EEPROM must update this pointer immediately following this
  first address byte.
