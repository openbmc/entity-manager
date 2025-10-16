cpp_args_fd = cpp_args
if get_option('fru-device-resizefru')
    cpp_args_fd = cpp_args_fd + ['-DENABLE_FRU_AREA_RESIZE']
endif
detect_mode = get_option('fru-device-16bitdetectmode')

cpp_args_fd += [
    '-DFRU_DEVICE_16BITDETECTMODE="' + detect_mode + '"',
    '-DENABLE_FRU_UPDATE_PROPERTY=' + (get_option('fru-device-update-property') == 'all' ? '1' : '0'),
]

executable(
    'fru-device',
    'fru_device.cpp',
    'fru_reader.cpp',
    'fru_utils.cpp',
    'gzip_utils.cpp',
    cpp_args: cpp_args_fd,
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
    link_with: utils_lib,
    install: true,
    install_dir: installdir,
)


