cpp_args = boost_args + ['-DPACKAGE_DIR="' + packagedir + '/"']

cpp_args_em = cpp_args
if get_option('entity-association-i2c')
  cpp_args_em = cpp_args_em + ['-DENABLE_ENTITY_ASSOCIATION_I2C']
endif

executable(
    'entity-manager',
    'EntityManager.cpp',
    'Overlay.cpp',
    'Utils.cpp',
    cpp_args: cpp_args_em + ['-DBOOST_ASIO_DISABLE_THREADS'],
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
        'FruDevice.cpp',
        'Utils.cpp',
        'FruUtils.cpp',
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
