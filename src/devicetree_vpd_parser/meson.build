cpp_args_fd = cpp_args
executable(
    'devicetree-vpd-parser',
    'machine_context.cpp',
    'devicetree_vpd_parser.cpp',
    cpp_args: cpp_args_fd,
    dependencies: [sdbusplus, phosphor_dbus_interfaces_dep],
    install: true,
    install_dir: installdir,
)

