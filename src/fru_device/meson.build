cpp_args_fd = cpp_args
if get_option('fru-device-resizefru')
    cpp_args_fd = cpp_args_fd + ['-DENABLE_FRU_AREA_RESIZE']
endif
detect_mode = get_option('fru-device-16bitdetectmode')
cpp_args_fd += ['-DFRU_DEVICE_16BITDETECTMODE="' + detect_mode + '"']
executable(
    'fru-device',
    'fru_device.cpp',
    '../utils.cpp',
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


