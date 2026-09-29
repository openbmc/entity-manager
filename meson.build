project(
    'entity-manager',
    'cpp',
    default_options: ['warning_level=3', 'werror=true', 'cpp_std=c++23'],
    license: 'Apache-2.0',
    version: '0.1',
    meson_version: '>=1.3.0',
)
add_project_arguments(
    ['-Wno-psabi', '-DJSON_USE_IMPLICIT_CONVERSIONS=0'],
    language: 'cpp',
)

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
    boost = declare_dependency(include_directories: 'subprojects/boost_1_88_0')
    boost = boost.as_system('system')
endif
if get_option('fru-device')
    i2c = cpp.find_library('i2c')
endif

phosphor_dbus_interfaces_dep = dependency(
    'phosphor-dbus-interfaces',
    include_type: 'system',
)

nlohmann_json_dep = dependency('nlohmann_json', include_type: 'system')
sdbusplus = dependency('sdbusplus')
phosphor_logging_dep = dependency('phosphor-logging')
zlib_dep = dependency('zlib', include_type: 'system')
libxml2_dep = dependency('libxml-2.0', include_type: 'system')

if get_option('gpio-presence') or get_option('tests').allowed()
    libgpio_dep = dependency(
        'libgpiodcxx',
        default_options: ['bindings=cxx'],
        version: '<1.7.0',
    )
endif

systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_variable(
    'systemd_system_unit_dir',
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
package_configdir = join_paths(packagedir, 'configurations')
fs = import('fs')

foreach c : configs
    file = join_paths('configurations', c)
    install_data(
        file,
        install_dir: join_paths(
            package_configdir,
            fs.parent(fs.relative_to(file, 'configurations')),
        ),
    )
    filepaths += [file]
endforeach

# this creates the 'schemas' variable
subdir('schemas')

schema_filepaths = []
foreach s : schemas
    schema_filepaths += [join_paths('schemas', s)]
    install_data(
        join_paths('schemas', s),
        install_dir: join_paths(packagedir, 'schemas', fs.parent(s)),
    )
endforeach

subdir('service_files')
subdir('src')

if get_option('validate-json')
    expected_fails = files('test/expected-schema-errors.txt')
    validation_command = [
        find_program('env'),
        'LG2_FORCE_STDERR=1',
        config_validator,
        '--schema',
        files('schemas/global.json')[0],
        '--expected-fails',
        expected_fails[0],
        '--continue',
        '--verbose',
    ]
    foreach config_file : files(filepaths)
        validation_command += ['--config', config_file]
    endforeach
    custom_target(
        'check_syntax',
        command: validation_command,
        depend_files: files(filepaths + schema_filepaths) + expected_fails,
        build_by_default: true,
        capture: true,
        output: 'validate_configs.log',
    )
endif

if get_option('tests').allowed()
    subdir('test')
endif
