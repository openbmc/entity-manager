# Template variable substituition in config JSON

In entity-manager config JSON there is a concept of having template variables which acts as placeholder and gets substituited with the property value at runtime.

## Template variable prefixed @

This document explains about @{template_variable} in Entity-Manager config JSON. The idea behind this is to extract the numeric value from a property with alphanumerals and substitute at the template variable prefixed with @ in Entity-Manager config JSONs.

## Purpose
On many server platforms, there can be multiple instances of same fru each are uniquely identified by the instance numbers. To keep the stable instance number that can be reused across all related objects for that FRU, we need to extract numeric token from it's name.

This mechanism enables dynamic derivation of numeric values from alphanumeric properties, allowing configuration entries to remain flexible and reusable without requiring explicit numeric values to be hard‑coded.


**Example**:
`
/xyz/openbmc_project/inventory/system/chassis/assembly/cpu1
├─ maps to → /xyz/openbmc_project/led/cpu1
├─ maps to → /xyz/openbmc_project/temp/cpu1
├─ maps to → /redfish/v1/Chassis/cpu1
`

## JSON Requirements

### JSON syntax:

In the entity‑manager configuration JSON, a property value can be specified using the format:

`@{template_variable}`

When a property value is defined using this syntax, the entity‑manager understands that a numeric value must be extracted from the referenced template_variable and substituted at runtime in place of @{template_variable}.

### Example configuration

`
{
    "Name": "PCIE SLOT",
    "Probe": "xyz.openbmc_project.FruDevice({'Name': '.*SLOT*'})",
    "Type": "Board",
    "xyz.openbmc_project.Inventory.Decorator.Asset": {
        "BuildDate": "$BOARD_MANUFACTURE_DATE",
        "Manufacturer": "$BOARD_MANUFACTURER",
        "Model": "$BOARD_PRODUCT_NAME",
        "PartNumber": "$BOARD_PART_NUMBER",
        "SerialNumber": "$BOARD_SERIAL_NUMBER"
        "SlotNumber": "@{Name}"
}
`

`
{
    "Name": "CPU",
    "Probe": "xyz.openbmc_project.FruDevice",
    "Type": "Board",
    "xyz.openbmc_project.Inventory.Decorator.Asset": {
        "BuildDate": "$BOARD_MANUFACTURE_DATE",
        "Manufacturer": "$BOARD_MANUFACTURER",
        "Model": "$BOARD_PRODUCT_NAME",
        "PartNumber": "$BOARD_PART_NUMBER",
        "SerialNumber": "$BOARD_SERIAL_NUMBER",
        "InstanceID": "@{Name}"
}
`

## Runtime Behavior

When the entity‑manager encounters a value in the form @{PropertyName}, it performs the following steps at runtime:

1. Searches for the specified PropertyName in the interface defined by the Probe.
2. Upon a successful match, retrieves the value of the corresponding property.
3. Extracts the first numeric sequence found within the property value.
4. Substitutes the extracted number in place of @{PropertyName}.

For example, if the Name property from the probe interface contains an alphanumeric value such as PCIE SLOT 3, the entity‑manager extracts 3 and assigns it to the SlotNumber property. 
