
executable('gpio-presence-sensor',
  'config.cpp',
  'gpio_presence.cpp',
  'gpio_provider.cpp',
  'config_provider.cpp',
  'main.cpp',
  'pdi-gen/DevicePresence/server.cpp',
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
