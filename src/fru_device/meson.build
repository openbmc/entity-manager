conf_data = configuration_data()
resize = get_option('fru-device-resizefru')
conf_data.set('ENABLE_FRU_AREA_RESIZE', resize.to_string().to_lower())

detect_mode = get_option('fru-device-16bitdetectmode')
conf_data.set('FRU_DEVICE_16BITDETECTMODE', detect_mode)

property_update = get_option('fru-device-update-property') == 'all'
conf_data.set(
    'ENABLE_FRU_UPDATE_PROPERTY',
    property_update.to_string().to_lower(),
)

conf_data.set('PACKAGE_DIR', packagedir)

fru_device_conf_dep = declare_dependency(
    include_directories: include_directories('.'),
    sources: configure_file(
        input: 'config.hpp.in',
        output: 'config.hpp',
        configuration: conf_data,
    ),
)

executable(
    'fru-device',
    'fru_device.cpp',
    '../utils.cpp',
    'fru_reader.cpp',
    'fru_utils.cpp',
    'gzip_utils.cpp',
    cpp_args: cpp_args,
    dependencies: [
        boost,
        fru_device_conf_dep,
        i2c,
        libxml2_dep,
        nlohmann_json_dep,
        phosphor_logging_dep,
        sdbusplus,
        threads,
        valijson,
        zlib_dep,
    ],
    install: true,
    install_dir: installdir,
)


