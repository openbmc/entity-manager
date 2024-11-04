
executable(
    'gpio-presence-sensor',
    'config.cpp',
    'gpio_presence.cpp',
    'config_provider.cpp',
    'main.cpp',
    dependencies: [
        boost,
        phosphor_logging_dep,
        phosphor_dbus_interfaces_dep,
        sdbusplus,
        gpiodcxx,
    ],
    install: true,
    install_dir: installdir,
)
