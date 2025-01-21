cpp_args = boost_args + ['-DPACKAGE_DIR="' + packagedir + '/"']
cpp_args += ['-DSYSCONF_DIR="' + sysconfdir + '/"']
installdir = join_paths(get_option('libexecdir'), 'entity-manager')

if get_option('new-device-detection')
    cpp_args += ['-DEM_CACHE_CONFIGURATION=true']
else
    cpp_args += ['-DEM_CACHE_CONFIGURATION=false']
endif

executable(
    'entity-manager',
    'entity_manager.cpp',
    'expression.cpp',
    'perform_scan.cpp',
    'perform_probe.cpp',
    'overlay.cpp',
    'topology.cpp',
    'utils.cpp',
    cpp_args: cpp_args + ['-DBOOST_ASIO_DISABLE_THREADS'],
    dependencies: [
        boost,
        nlohmann_json_dep,
        phosphor_logging_dep,
        sdbusplus,
        valijson,
    ],
    install: true,
    install_dir: installdir,
)

if get_option('fru-device')
    cpp_args_fd = cpp_args
    if get_option('fru-device-resizefru')
        cpp_args_fd = cpp_args_fd + ['-DENABLE_FRU_AREA_RESIZE']
    endif
    detect_mode = get_option('fru-device-16bitdetectmode')
    cpp_args_fd += ['-DFRU_DEVICE_16BITDETECTMODE="' + detect_mode + '"']
    executable(
        'fru-device',
        'expression.cpp',
        'fru_device.cpp',
        'utils.cpp',
        'fru_utils.cpp',
        'fru_reader.cpp',
        cpp_args: cpp_args_fd,
        dependencies: [
            boost,
            i2c,
            nlohmann_json_dep,
            phosphor_logging_dep,
            sdbusplus,
            threads,
            valijson,
        ],
        install: true,
        install_dir: installdir,
    )
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
