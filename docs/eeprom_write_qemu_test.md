# Testing EEPROM writes in QEMU

This guide describes a reusable way to exercise EEPROM write success and failure
behavior in an isolated OpenBMC QEMU guest. Adapt the machine, image, EEPROM
bus/address, D-Bus method, and valid FRU payload to the target under test. These
steps test application behavior; injected results are not evidence of physical
EEPROM or driver behavior.

The expected failure responses below require an entity-manager version that
detects incomplete EEPROM writes, such as the version in
[Gerrit change 97439](https://gerrit.openbmc.org/c/openbmc/entity-manager/+/97439).

## Build and boot an image

Follow the OpenBMC [development environment guide][dev-environment] to build an
image. The guide uses Romulus as an example; select a supported machine for your
target. To include local entity-manager changes, follow the OpenBMC [devtool
tutorial][devtool-tutorial], rebuild the image, and verify it contains the
intended source revision.

Follow the development environment guide's [QEMU download and launch
instructions][qemu-guide]. Use the QEMU machine type and matching image for your
target; the Romulus command in that guide is an example, not a universal
command. Use a disposable guest and an image copy or snapshot drive. Record the
image and module revisions, build configuration, QEMU version, machine type, and
forwarded ports with the test results.

## Set up an isolated EEPROM fixture

Run this setup inside the disposable guest. Choose a test bus and address that
do not collide with devices exposed in the guest. The sysfs device name uses the
bus number and four-digit hexadecimal address; `busctl` takes the address as a
decimal byte. The bus and address below are examples; choose values available in
the guest.

```sh
export FRU_BUS=99
export FRU_ADDR=80
FRU_ADDR_HEX=$(printf '%04x' "$FRU_ADDR")
export FRU_DEVICE="${FRU_BUS}-${FRU_ADDR_HEX}"
export FRU_PATH="/sys/bus/i2c/devices/$FRU_DEVICE/eeprom"
export FRU_EEPROM_MATCH="/$FRU_DEVICE/eeprom"
export FIXTURE_ROOT=/tmp/eeprom-test
export FIXTURE_EEPROM="$FIXTURE_ROOT/$FRU_DEVICE/eeprom"
export SERVICE=xyz.openbmc_project.FruDevice.service
systemctl stop "$SERVICE"
mkdir -p "$(dirname "$FIXTURE_EEPROM")"
mount --bind "$FIXTURE_ROOT" /sys/bus/i2c/devices
```

For environments where a full-directory bind mount is unsuitable, provide an
equivalent isolated device fixture without hiding unrelated devices. Do not
mount over sysfs or inject failures on a production BMC.

## Inject write outcomes

The following guest-only `LD_PRELOAD` wrapper intercepts `write()` only when the
opened descriptor path contains `FRU_EEPROM_MATCH`, a unique suffix for the
fixture. The descriptor may resolve to the sysfs path or the backing-file path.
All other writes are passed through. Compile it for the guest ABI with the
target cross-toolchain and sysroot, then copy the shared library into the guest.

```c
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

ssize_t write(int fd, const void* data, size_t count)
{
    ssize_t (*real_write)(int, const void*, size_t) =
        dlsym(RTLD_NEXT, "write");
    char name[64];
    char path[512];
    snprintf(name, sizeof(name), "/proc/self/fd/%d", fd);
    ssize_t length = readlink(name, path, sizeof(path) - 1);
    if (length >= 0)
    {
        path[length] = '\0';
        const char* mode = getenv("FRU_WRITE_MODE");
        const char* target = getenv("FRU_EEPROM_MATCH");
        if (mode && target && strstr(path, target))
        {
            if (!strcmp(mode, "short") && count > 0)
            {
                return real_write(fd, data, count - 1);
            }
            if (!strcmp(mode, "zero"))
            {
                return 0;
            }
            if (!strcmp(mode, "error"))
            {
                errno = EIO;
                return -1;
            }
        }
    }
    return real_write(fd, data, count);
}
```

Save the wrapper as `eeprom-write-inject.c`. Compile for the selected target,
adapting the compiler, ABI flags, and sysroot to the image toolchain:

```sh
export TARGET_CC=/path/to/target-cross-gcc
export TARGET_SYSROOT=/path/to/target-sysroot
export TARGET_ABI_FLAGS='' # Add target-specific ABI flags if needed.
"$TARGET_CC" --sysroot="$TARGET_SYSROOT" \
    $TARGET_ABI_FLAGS -shared -fPIC \
    -o eeprom-write-inject.so eeprom-write-inject.c -ldl
```

Copy the injector into the guest using an available transfer method. Install a
temporary systemd drop-in to set `LD_PRELOAD`, `FRU_EEPROM_MATCH`, and
`FRU_WRITE_MODE` for the daemon. The modes below simulate syscall results; they
do not simulate device-specific timing or electrical behavior:

- `full`: call the real write unchanged.
- `short`: write one byte fewer than requested and return that count.
- `zero`: return zero without writing.
- `error`: return `-1` with `errno` set to `EIO`.

## Exercise the D-Bus method

Use a structurally valid FRU payload accepted by the target daemon. Set the bus,
address, payload size, and bytes to match that fixture. `FRU_BYTES` is a
space-separated list of decimal bytes; replace the placeholder with the complete
payload for the target.

```sh
export FRU_SIZE=176 # Replace with the selected payload length.
export FRU_BYTES='<decimal-byte-0> <decimal-byte-1> ...'
```

For each case, stop the daemon, reset the backing file, write the systemd
drop-in with the desired mode, reload systemd, clear any prior start-limit
state, and start the service:

```sh
systemctl stop "$SERVICE"
FIXTURE_SIZE=$FRU_SIZE
# For capacity rejection, set FIXTURE_SIZE to FRU_SIZE minus one.
dd if=/dev/zero of="$FIXTURE_EEPROM" bs=1 count="$FIXTURE_SIZE" 2>/dev/null
# Set the mode to full, short, zero or error for each test case.
export FRU_WRITE_MODE=full
mkdir -p "/run/systemd/system/$SERVICE.d"
cat > "/run/systemd/system/$SERVICE.d/write-test.conf" <<EOF
[Service]
Environment=LD_PRELOAD=/path/in/guest/eeprom-write-inject.so
Environment=FRU_EEPROM_MATCH=$FRU_EEPROM_MATCH
Environment=FRU_WRITE_MODE=$FRU_WRITE_MODE
EOF
systemctl daemon-reload
systemctl reset-failed "$SERVICE"
systemctl start "$SERVICE"
```

In the guest shell, pass each byte as a separate D-Bus array argument. Replace
the byte-list placeholder with all payload bytes. This POSIX-shell form works
without Bash arrays:

```sh
set -f
set -- $FRU_BYTES
test "$#" -eq "$FRU_SIZE"
busctl --timeout=30 call \
    xyz.openbmc_project.FruDevice \
    /xyz/openbmc_project/FruDevice \
    xyz.openbmc_project.FruDeviceManager \
    WriteFru qyay "$FRU_BUS" "$FRU_ADDR" "$FRU_SIZE" "$@"
```

Exercise these cases:

1. **Full write:** use a fixture at least `FRU_SIZE` bytes long and mode `full`.
   Expect D-Bus success and verify the bytes match the payload.
2. **Known-capacity rejection:** make the fixture smaller than `FRU_SIZE`; use
   mode `full`. Expect D-Bus failure and verify the file is unchanged and no
   write was attempted.
3. **Short write:** use a large enough fixture and mode `short`. Expect D-Bus
   failure; verify the reported count and inspect the file for partial side
   effects.
4. **Zero write:** use a large enough fixture and mode `zero`. Expect D-Bus
   failure and verify the file is unchanged.
5. **Write error:** use a large enough fixture and mode `error`. Expect D-Bus
   failure, an `EIO` diagnostic, and an unchanged file.

After each call, record the command and exit status, D-Bus response, file
contents or checksum, daemon state, and relevant logs:

```sh
systemctl is-active "$SERVICE"
journalctl -u "$SERVICE" -n 25 --no-pager
```

Check exact file bytes when possible; a checksum alone does not identify which
part of a partially written file changed. Confirm that existing D-Bus failure
responses remain intact. A short write may already have modified EEPROM bytes;
reporting failure does not roll back those bytes. Also check whether the failure
path skips any rescan or cache update.

Seek and metadata (`fstat`) failures can be covered by focused unit tests or by
adding targeted injection to the test harness. Do not report those paths as
QEMU-tested unless they were actually exercised.

## Cleanup

Remove the temporary drop-in, restore the original device view, reload systemd,
and start the service before continuing guest tests:

```sh
systemctl stop "$SERVICE"
rm -f "/run/systemd/system/$SERVICE.d/write-test.conf"
umount /sys/bus/i2c/devices
systemctl daemon-reload
systemctl reset-failed "$SERVICE"
systemctl start "$SERVICE"
```

Stop QEMU after testing. Use a snapshot drive so test writes are discarded when
the guest exits. Report the exact setup and observed results; distinguish
injected syscall behavior from real hardware tests.

[dev-environment]:
  https://github.com/openbmc/docs/blob/master/development/dev-environment.md#building-openbmc
[devtool-tutorial]:
  https://github.com/openbmc/docs/blob/master/development/devtool-hello-world.md
[qemu-guide]:
  https://github.com/openbmc/docs/blob/master/development/dev-environment.md#download-and-start-qemu-session
