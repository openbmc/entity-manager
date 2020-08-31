# Entity Manager

Entity manager is a design for managing physical system components, and mapping
them to software resources within the BMC.  Said resources are designed to allow
the flexible adjustment of the system at runtime, as well as the reduction in
the number of independent system configurations one needs to create.

## Definitions

### Entity
A server component that is physically separate, detectable through some means,
and can be added or removed from a given OpenBMC system.  Said component can,
and likely does contain multiple sub components, but the component itself as a
whole is referred to as an entity.

Note, this term is needed because most other term that could've been used
(Component, Field Replaceable Unit, or Assembly) are already overloaded in the
industry, and have a distinct definition already, which is a subset of what an
entity encompasses.

### Exposes
A particular feature of an Entity.  An Entity generally will have multiple
exposes records for the various features that component supports.  Some examples
of features include, LM75 sensors, PID control parameters, or CPU information.

### Probe
A set of rules for detecting a given entity.  Said rules generally take the form
of a DBus interface definition.


## Goals
Entity manager has the following goals (unless you can think of better ones):

1. Minimize the time and debugging required to "port" OpenBMC to new systems
2. Reduce the amount of code that is different between platforms
3. Create system level maintainability in the long term, across hundreds of
   platforms and components, such that components interoperate as much as
   physically possible.

## Implementation
A full BMC setup using Entity Manager consists of a few parts:

1. **A detection daemon**  This is something that can be used to detect
   components at runtime.  The most common of these, Fru-Device, is included in
   the Entity-Manager repo, and scans all available I2C buses IPMI FRU EEPROM
   devices.  Other examples of detection daemons include:
   **peci-pcie:**
   A daemon that utilizes the CPU bus to read in a list of PCIe devices from
   the processor.
   **smbios-mdr:**
   A daemon that utilizes the x86 SMBIOS table specification to detect the
   available systems dependencies from BIOS.

   In many cases, the existing detection daemons are sufficient for a single
   system, but in cases where there is a superseding inventory control system in
   place (such as in a large datacenter) they can be replaced with application
   specific daemons that speak the protocol information of their controller, and
   expose the inventory information, such that failing devices can be detected
   more readily, and system configurations can be "verified" rather than
   detected.

2. **An entity manager configuration file**  Entity manager configuration files
   are located in the /configurations directory in the entity manager
   repository, and include one file per device supported.  Entities are
   detected based on the "Probe" key in the json file.  The intention is that
   this folder contains all hardware configurations that OpenBMC supports, to
   allows an easy answer to "Is X device supported".  An EM configuration
   contains a number of exposes records that specify the specific features that
   this Entity supports.  Once a component is detected, entity manager will
   publish these exposes records to DBus.  In some cases, fields may be passed
   through from the detection daemon to the entity manager DBus interface.
   This is done for efficiency of DBus transactions and to avoid bugs creation
   order issues, but ideally should be avoided if possible.

3. **A reactor**  The reactors are things that take the entity manager
   configurations, and use them to execute and enable the features that they
   describe.  One example of this is dbus-sensors, which contains a suite of
   applications that input the exposes records for sensor devices, then connect
   to the filesystem to create the sensors and scan loops to scan sensors for
   those devices.  Other examples of reactors could include: CPU management
   daemons and Hot swap backplane management daemons, or drive daemons.

**note:**  In some cases, a given daemon could be both a detection daemon and a
reactor when architectures are multi-tiered.  An example of this might include a
hot swap backplane daemon, which both reacts to the hot swap being detected, and
also creates detection records of what drives are present.

## Requirements

1. Entity manager shall support the dynamic discovery of hardware at runtime,
   using inventory interfaces.  The types of devices include, but are not
   limited to hard drives, hot swap backplanes, baseboards, power supplies,
   CPUs, and PCIe Add-in-cards.

2. Entity manager shall support the ability to add or remove support for
   particular devices in a given binary image.  By default, entity manager will
   support all available and known working devices for all platforms.

3. Entity manager shall provide data to DBus about a particular device such that
   other daemons can create instances of the features being exposed.

4. Entity manager shall support multiple detection runs, and shall do the
   minimal number of changes necessary when new components are detected or no
   longer detected.  Some examples of re-detection events might include host
   power on, drive plug/unplug, PSU plug/unplug.

5. Entity manager shall have exactly one configuration file per supported device
   model.  In some cases this will cause duplicated information between files,
   but the ability to list and see all supported device models in a single
   place, as well as maintenance when devices do differ in the future is
   determined to be more important than duplication of configuration files.


### Explicitly out of scope
1. Entity manager shall not directly participate in the detection of devices,
   and instead will rely on other DBus applications to publish interfaces that
   can be detected.
2. Entity manager shall not directly participate in management of any specific
   device.  This is requirement is intended to intentionally limit the size and
   feature set of entity manager, to ensure it remains small, and effective to
   all users.

