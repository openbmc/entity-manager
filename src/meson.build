cpp_args = boost_args + ['-DPACKAGE_DIR="' + packagedir + '/"']
cpp_args += ['-DSYSCONF_DIR="' + sysconfdir + '/"' ]

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
        sdbusplus,
        valijson,
    ],
    implicit_include_directories: false,
    include_directories: '../include',
    install: true,
)

if get_option('fru-device')
    cpp_args_fd = cpp_args
    if get_option('fru-device-resizefru')
        cpp_args_fd = cpp_args_fd + ['-DENABLE_FRU_AREA_RESIZE']
    endif
    executable(
        'fru-device',
        'expression.cpp',
        'fru_device_main.cpp',
        'fru_device.cpp',
        'utils.cpp',
        'fru_utils.cpp',
        'fru_reader.cpp',
        'ipmb_fru_device.cpp',
        cpp_args: cpp_args_fd,
        dependencies: [
            boost,
            i2c,
            nlohmann_json_dep,
            sdbusplus,
            threads,
            valijson,
        ],
        implicit_include_directories: false,
        include_directories: '../include',
        install: true,
    )
endif


if get_option('ipmb-fru-device')
    cpp_args_fd = cpp_args
    if get_option('ipmb-fru-device-resizefru')
        cpp_args_fd = cpp_args_fd + ['-DENABLE_FRU_AREA_RESIZE']
    endif
    executable(
        'ipmb-fru-device',
        'ipmb_fru_device_main.cpp',
        'expression.cpp',
        'ipmb_fru_device.cpp',
        'utils.cpp',
        'fru_utils.cpp',
        'fru_reader.cpp',
        'fru_device.cpp',
        cpp_args: cpp_args,
        dependencies: [
            boost,
            i2c,
            sdbusplus,
            threads,
            valijson,
      ],
      implicit_include_directories: false,
      include_directories: '../include',
      install: true,
    )
endif
