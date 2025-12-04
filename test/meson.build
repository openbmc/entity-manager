test_boost_args = boost_args + ['-DBOOST_ASIO_DISABLE_THREADS'] + [
    '-DENABLE_FRU_UPDATE_PROPERTY=1',
]
gtest = dependency('gtest', main: true, disabler: true, required: false)
gmock = dependency('gmock', disabler: true, required: false)

if not (gtest.found() and gmock.found())
    cmake = import('cmake')
    gtest_subproject = cmake.subproject('gtest')
    cm_gtest = gtest_subproject.dependency('gtest')
    cm_gtest_main = gtest_subproject.dependency('gtest_main')
    gtest = declare_dependency(dependencies: [cm_gtest, cm_gtest_main, threads])
    gmock = gtest_subproject.dependency('gmock')

endif

test_include_dir = include_directories('../src')

subdir('entity_manager')

test(
    'test_fru_utils',
    executable(
        'test_fru_utils',
        'test_fru-utils.cpp',
        'test_gzip-utils.cpp',
        '../src/dbus_regex.cpp',
        '../src/fru_device/fru_utils.cpp',
        '../src/fru_device/fru_reader.cpp',
        '../src/fru_device/gzip_utils.cpp',
        cpp_args: test_boost_args,
        dependencies: [
            boost,
            gmock,
            gtest,
            libxml2_dep,
            phosphor_logging_dep,
            sdbusplus,
            zlib_dep,
        ],
        include_directories: test_include_dir,
    ),
)

test(
    'test_topology',
    executable(
        'test_topology',
        'test_topology.cpp',
        '../src/entity_manager/topology.cpp',
        cpp_args: test_boost_args,
        dependencies: [gtest, gmock, nlohmann_json_dep, phosphor_logging_dep],
        include_directories: test_include_dir,
    ),
)

test(
    'test_gpio_presence',
    executable(
        'test_gpio_presence',
        'test_gpio_presence.cpp',
        cpp_args: test_boost_args,
        include_directories: test_include_dir,
        dependencies: [boost, gtest, gmock, phosphor_logging_dep, libgpio_dep],
        link_with: gpio_presence_lib,
    ),
)

test(
    'test_utils',
    executable(
        'test_utils',
        'test_utils.cpp',
        '../src/utils.cpp',
        include_directories: test_include_dir,
        dependencies: [gtest, phosphor_logging_dep, sdbusplus],
    ),
)

test(
    'test_gpio_presence_state',
    executable(
        'test_gpio_presence_state',
        'test_gpio_presence_state.cpp',
        cpp_args: test_boost_args,
        include_directories: test_include_dir,
        dependencies: [boost, gtest, gmock, phosphor_logging_dep, libgpio_dep],
        link_with: gpio_presence_lib,
    ),
)
