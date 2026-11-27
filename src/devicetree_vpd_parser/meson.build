cpp_args_fd = cpp_args
executable(
    'devicetree-vpd-parser',
    'machine_context.cpp',
    'devicetree_vpd_parser.cpp',
    cpp_args: cpp_args_fd,
    dependencies: [
        phosphor_dbus_interfaces_dep,
        phosphor_logging_dep,
        sdbusplus,
    ],
    install: true,
    install_dir: installdir,
)

