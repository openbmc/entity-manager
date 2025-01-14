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
build_tests = get_option('tests')
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
    'modbus.json',
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

if not build_tests.disabled()
    test_boost_args = boost_args + ['-DBOOST_ASIO_DISABLE_THREADS']
    gtest = dependency('gtest', main: true, disabler: true, required: false)
    gmock = dependency('gmock', disabler: true, required: false)
    if not (gtest.found() and gmock.found()) and build_tests.enabled()
        cmake = import('cmake')
        gtest_subproject = cmake.subproject('gtest')
        cm_gtest = gtest_subproject.dependency('gtest')
        cm_gtest_main = gtest_subproject.dependency('gtest_main')
        gtest = declare_dependency(
            dependencies: [cm_gtest, cm_gtest_main, threads],
        )
        gmock = gtest_subproject.dependency('gmock')

    endif

    test(
        'test_entity_manager',
        executable(
            'test_entity_manager',
            'test/test_entity-manager.cpp',
            'src/entity_manager/expression.cpp',
            'src/utils.cpp',
            'src/entity_manager/utils.cpp',
            cpp_args: test_boost_args,
            dependencies: [
                boost,
                gtest,
                nlohmann_json_dep,
                phosphor_logging_dep,
                sdbusplus,
                valijson,
            ],
            include_directories: 'src',
        ),
    )

    test(
        'test_fru_utils',
        executable(
            'test_fru_utils',
            'test/test_fru-utils.cpp',
            'src/fru_device/fru_utils.cpp',
            'src/fru_device/fru_reader.cpp',
            cpp_args: test_boost_args,
            dependencies: [boost, gtest, gmock, phosphor_logging_dep, sdbusplus],
            include_directories: 'src',
        ),
    )

    test(
        'test_topology',
        executable(
            'test_topology',
            'test/test_topology.cpp',
            'src/entity_manager/topology.cpp',
            cpp_args: test_boost_args,
            dependencies: [
                gtest,
                gmock,
                nlohmann_json_dep,
                phosphor_logging_dep,
            ],
            include_directories: 'src',
        ),
    )
endif
