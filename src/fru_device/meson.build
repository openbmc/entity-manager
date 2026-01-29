cpp_args_fd = cpp_args
if get_option('fru-device-resizefru')
    cpp_args_fd = cpp_args_fd + ['-DENABLE_FRU_AREA_RESIZE']
endif
detect_mode = get_option('fru-device-16bitdetectmode')
max_retries = get_option('fru-device-maxretries')
retry_delay = get_option('fru-device-retrydelay')

if max_retries > 50
    error('fru-device-maxretries is too large')
endif

if retry_delay < 1
    error('fru-device-retrydelay is too small')
endif

cpp_args_fd += [
    '-DFRU_DEVICE_16BITDETECTMODE="' + detect_mode + '"',
    '-DENABLE_FRU_UPDATE_PROPERTY=' + (
        get_option('fru-device-update-property') == 'all' ? '1' : '0'
    ),
    '-DFRU_DEVICE_MAXRETRIES=' + max_retries.to_string(),
    '-DFRU_DEVICE_RETRYDELAY=' + retry_delay.to_string(),
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
        stdplus,
        threads,
        valijson,
        zlib_dep,
    ],
    link_with: utils_lib,
    install: true,
    install_dir: installdir,
)


