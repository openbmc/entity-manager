gpio_presence_lib = static_library(
    'gpio_presence_lib',
    'device_presence.cpp',
    'device_presence.hpp',
    'gpio_presence_manager.cpp',
    'gpio_presence_manager.hpp',
    'config_provider.cpp',
    'config_provider.hpp',
    dependencies: [
        boost,
        phosphor_logging_dep,
        phosphor_dbus_interfaces_dep,
        sdbusplus,
        libgpio_dep,
    ],
)

executable(
    'gpio-presence-sensor',
    'main.cpp',
    dependencies: [
        boost,
        phosphor_logging_dep,
        phosphor_dbus_interfaces_dep,
        sdbusplus,
        libgpio_dep,
    ],
    install: true,
    install_dir: installdir,
    link_with: gpio_presence_lib,
)
