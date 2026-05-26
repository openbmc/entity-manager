cpp_args_fd += [
    '-DENABLE_FRU_UPDATE_PROPERTY=' + (
        get_option('fru-device-update-property') == 'all' ? '1' : '0'
    ),
]

executable(
    'nfc-fru-parser',
    'nfc_fru_parser.cpp',
    'neard_dbus.cpp',
    '../fru_device/fru_utils.cpp',
    '../fru_device/fru_reader.cpp',
    '../fru_device/gzip_utils.cpp',
    cpp_args: cpp_args_fd,
    dependencies: [
        sdbusplus,
        phosphor_logging_dep,
        libxml2_dep,
        zlib_dep,
    ],
    link_with: utils_lib,
    install: true,
    install_dir: installdir,
)
