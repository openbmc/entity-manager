cpp_args = boost_args + ['-DPACKAGE_DIR="' + packagedir + '/"']
cpp_args += ['-DSYSCONF_DIR="' + sysconfdir + '/"']
installdir = join_paths(get_option('libexecdir'), 'entity-manager')

subdir('entity_manager')

if get_option('fru-device')
    subdir('fru_device')
endif

if get_option('devicetree-vpd')
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
endif
