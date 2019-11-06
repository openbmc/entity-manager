project(
    'entity-manager',
    'cpp',
    default_options: [
        'warning_level=3',
        'werror=true',
        'cpp_std=c++17'
    ],
    license: 'Apache-2.0',
    version: '0.1',
)

boost_args = [
    '-DBOOST_SYSTEM_NO_DEPRECATED',
    '-DBOOST_ERROR_CODE_HEADER_ONLY',
    '-DBOOST_NO_RTTI',
    '-DBOOST_NO_TYPEID',
    '-DBOOST_ALL_NO_LIB',
]
build_tests = get_option('tests')
i2c = meson.get_compiler('cpp').find_library('i2c')
sdbusplus = dependency('sdbusplus')
systemd = dependency('systemd')
systemd_system_unit_dir = systemd.get_pkgconfig_variable(
    'systemdsystemunitdir',
    define_variable: ['prefix', get_option('prefix')])
packagedir = join_paths(
    get_option('datadir'),
    meson.project_name(),
)
threads = dependency('threads')

install_data('blacklist.json')

subdir('configurations')
subdir('overlay_templates')
subdir('schemas')
subdir('service_files')
subdir('src')
subdir('test')
