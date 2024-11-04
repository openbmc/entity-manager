
gpiodcxx = dependency('libgpiodcxx', default_options: ['bindings=cxx'])

executable('gpio-presence-sensor',
  'Utils.hpp',
  'Config.cpp',
  'Config.hpp',
  'GPIOPresenceSensor.cpp',
  'GPIOPresenceSensor.hpp',
  'GPIOPresenceSensorMain.cpp',
  'Interfaces.hpp',
  dependencies: [
    boost,
    phosphor_logging_dep,
    sdbusplus,
    gpiodcxx,
  ],
  install: true,
  install_dir: installdir,
)
