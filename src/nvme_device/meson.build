nvme_utils_lib = static_library(
    'nvme_device_utils',
    'nvme_device_utils.cpp',
    cpp_args: cpp_args,
    dependencies: [boost, nlohmann_json_dep],
)

cpp_args_nvme = cpp_args + [
    '-DENABLE_FRU_UPDATE_PROPERTY=' + (
        get_option('fru-device-update-property') == 'all' ? '1' : '0'
    ),
]

executable(
    'nvme-device',
    'nvme_device.cpp',
    '../fru_device/fru_reader.cpp',
    '../fru_device/fru_utils.cpp',
    '../fru_device/gzip_utils.cpp',
    cpp_args: cpp_args_nvme,
    dependencies: [
        boost,
        i2c,
        libxml2_dep,
        nlohmann_json_dep,
        phosphor_logging_dep,
        sdbusplus,
        threads,
        valijson,
        zlib_dep,
    ],
    link_with: [utils_lib, nvme_utils_lib],
    install: true,
    install_dir: installdir,
)
