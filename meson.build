project(
    'entity-manager',
    'cpp',
    default_options: ['warning_level=3', 'werror=true', 'cpp_std=c++23'],
    license: 'Apache-2.0',
    version: '0.1',
    meson_version: '>=1.1.1',
)
add_project_arguments('-Wno-psabi', language: 'cpp')

boost_args = [
    '-DBOOST_ASIO_NO_DEPRECATED',
    '-DBOOST_SYSTEM_NO_DEPRECATED',
    '-DBOOST_ERROR_CODE_HEADER_ONLY',
    '-DBOOST_NO_RTTI',
    '-DBOOST_NO_TYPEID',
    '-DBOOST_ALL_NO_LIB',
    '-DBOOST_ALLOW_DEPRECATED_HEADERS',
]
cpp = meson.get_compiler('cpp')
boost = dependency('boost', required: false)
if not boost.found()
    subproject('boost', required: false)
    boost = declare_dependency(include_directories: 'subprojects/boost_1_71_0')
    boost = boost.as_system('system')
endif
if get_option('fru-device')
    i2c = cpp.find_library('i2c')
endif

if get_option('devicetree-vpd')
    phosphor_dbus_interfaces_dep = dependency(
        'phosphor-dbus-interfaces',
        include_type: 'system',
    )
endif

nlohmann_json_dep = dependency('nlohmann_json', include_type: 'system')
sdbusplus = dependency('sdbusplus', include_type: 'system')
phosphor_logging_dep = dependency('phosphor-logging')

systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_variable(
    'systemdsystemunitdir',
    pkgconfig_define: ['prefix', get_option('prefix')],
)
packagedir = join_paths(
    get_option('prefix'),
    get_option('datadir'),
    meson.project_name(),
)
sysconfdir = join_paths(
    get_option('prefix'),
    get_option('sysconfdir'),
    meson.project_name(),
)
threads = dependency('threads')
if cpp.has_header('valijson/validator.hpp')
    valijson = declare_dependency()
else
    subproject('valijson', required: false)
    valijson = declare_dependency(
        include_directories: 'subprojects/valijson/include',
    )
    valijson = valijson.as_system('system')
endif

install_data('blacklist.json')

# this creates the 'configs' variable
subdir('configurations')

filepaths = []
foreach c : configs
    file = join_paths('configurations', c)
    install_data(file, install_dir: join_paths(packagedir, 'configurations'))
    filepaths += [file]
endforeach

if get_option('validate-json')
    validate_script = files('scripts/validate_configs.py')
    autojson = custom_target(
        'check_syntax',
        command: [validate_script, '-v', '-k'],
        depend_files: files(filepaths),
        build_by_default: true,
        capture: true,
        output: 'validate_configs.log',
    )
endif

schemas = [
    'global.json',
    'legacy.json',
    'openbmc-dbus.json',
    'ibm.json',
    'intel.json',
    'mctp.json',
    'pid.json',
    'pid_zone.json',
    'stepwise.json',
    'virtual_sensor.json',
    'satellite_controller.json',
    'leak_detector.json',
    'firmware.json',
]

foreach s : schemas
    install_data(
        join_paths('schemas', s),
        install_dir: join_paths(packagedir, 'configurations', 'schemas'),
    )
endforeach

subdir('service_files')
subdir('src')

if get_option('tests').allowed()
    subdir('test')
endif
