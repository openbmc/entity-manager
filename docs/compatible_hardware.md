# Compatible Hardware

Entity-Manager configurations can contain a `CompatibleHardware` property to
determine the hardware they are compatible with or intended for.

## Configuration

Example for a Tyan s8047 baseboard configuration file:

```json
"CompatibleHardware": "tyan-s8047"
```

Example for a PSU configuration file:

```json
"CompatibleHardware": "psu-1u-supermicro-pws-920p-sq"
```

In case a configuration file should be present on all boards:

```json
"CompatibleHardware": ""
```

Currenly there is no fixed schema for the `CompatibleHardware` tags, it is
expected to emerge as the feature is used. For compatibility, the type of port
or connector should come first in most of these tags. Some ideas:

For baseboard:

```text
${manufacturer}-${baseboard_Name}
```

For OEM-specific hardware modules which will not work on boards from other
manufacturers:

```text
${manufacturer}-${module_name}
```

For psu:

```text
psu-${form factor}
```

For PCIe hardware:

```text
pcie-${manufacturer}-${module_name}
```

## Meson option

To use this feature, the meson option `compatible-hardware` is used. An empty
value will result in installing all configuration files.

The option accepts a space-separated list of regex to use for choosing
configuration.

Example for building EM with support only for IBM baseboards and IBM-specific
modules:

```text
-Dcompatible-hardware="ibm-*"
```

Example for building EM with support for Tyan S5549 baseboard, PCIE and PSU
configs:

```text
-Dcompatible-hardware="tyan-s5549 pcie-* psu-*"
```
