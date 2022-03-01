cpp_args = boost_args + ['-DPACKAGE_DIR="' + packagedir + '/"']
cpp_args += ['-DSYSCONF_DIR="' + sysconfdir + '/"' ]

executable(
    'entity-manager',
    'EntityManager.cpp',
    'Expression.cpp',
    'PerformScan.cpp',
    'PerformProbe.cpp',
    'Overlay.cpp',
    'Utils.cpp',
    cpp_args: cpp_args + ['-DBOOST_ASIO_DISABLE_THREADS'],
    dependencies: [
        boost,
        nlohmann_json,
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
        'Expression.cpp',
        'FruDevice.cpp',
        'Utils.cpp',
        'FruUtils.cpp',
        'spd.cpp',
        'ddr5.cpp',
        cpp_args: cpp_args_fd,
        dependencies: [
            boost,
            i2c,
            nlohmann_json,
            sdbusplus,
            threads,
            valijson,
        ],
        implicit_include_directories: false,
        include_directories: '../include',
        install: true,
    )
endif
