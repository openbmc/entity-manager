test_boost_args = boost_args + ['-DBOOST_ASIO_DISABLE_THREADS']
gtest = dependency('gtest', main: true, disabler: true, required: false)
gmock = dependency('gmock', disabler: true, required: false)
if not (gtest.found() and gmock.found())
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
        'test_entity-manager.cpp',
        '../src/expression.cpp',
        '../src/utils.cpp',
        cpp_args: test_boost_args,
        dependencies: [
            boost,
            gtest,
            nlohmann_json_dep,
            phosphor_logging_dep,
            sdbusplus,
            valijson,
        ],
        include_directories: '../src',
    ),
)

test(
    'test_fru_utils',
    executable(
        'test_fru_utils',
        'test_fru-utils.cpp',
        '../src/fru_utils.cpp',
        '../src/fru_reader.cpp',
        cpp_args: test_boost_args,
        dependencies: [boost, gtest, gmock, phosphor_logging_dep, sdbusplus],
        include_directories: '../src',
    ),
)

test(
    'test_topology',
    executable(
        'test_topology',
        'test_topology.cpp',
        '../src/topology.cpp',
        cpp_args: test_boost_args,
        dependencies: [
            gtest,
            gmock,
            nlohmann_json_dep,
            phosphor_logging_dep,
        ],
        include_directories: '../src',
    ),
)
