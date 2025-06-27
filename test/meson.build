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

test(
    'test_utils',
    executable(
        'test_utils',
        'utils.cpp',
        '../src/utils.cpp',
        include_directories: test_include_dir,
        dependencies: [gtest, phosphor_logging_dep, sdbusplus],
    ),
)

subdir('entity_manager')
subdir('fru_device')
subdir('gpio_presence')
