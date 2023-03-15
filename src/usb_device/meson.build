udev_dep = dependency('libudev', required: true)

executable(
    'usb-device',
    'usb_device.cpp',
    cpp_args: cpp_args,
    dependencies: [boost, sdbusplus, udev_dep],
    implicit_include_directories: false,
    install: true,
    install_dir: installdir,
)
